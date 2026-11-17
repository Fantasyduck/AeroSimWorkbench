#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include "aerosim/visualization/VtkSceneViewer.h"
#include <QApplication>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#include <QTimer>
#include <QTreeWidget>
#include <QDebug>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QListWidget>
#include <QFileInfo>
#include <QComboBox>
#include <QMessageBox>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkGenericOpenGLRenderWindow.h>
int main(int argc,char **argv) {
    if(argc!=2 && argc!=3) return 2;
    const bool meshOnly=argc==3 && QString::fromLocal8Bit(argv[2])==QStringLiteral("--mesh");
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc,argv);
    aerosim::PostProcessWorkbenchWindow window;
    auto *viewer=new aerosim::visualization::VtkSceneViewer;
    if(!window.insertWidget(aerosim::WidgetSlot::Viewport,viewer)) return 3;
    viewer->bindWorkbench(&window);
    auto *tree=window.findChild<QTreeWidget *>(QStringLiteral("postProcessDatasetTree"));
    if(!tree) return 4;
    window.show();
    int ticks=0, phase=0; QTimer poll;
    const QString path=QString::fromLocal8Bit(argv[1]);
    const auto submit=[&](const QString &filePath,int mode) {
        auto *action=window.findChild<QAction *>(QStringLiteral("postProcessImportAction"));
        if(!action) { app.exit(11); return; }
        action->trigger();
        auto *dialog=window.findChild<QDialog *>(QStringLiteral("postProcessImportDataDialog"));
        if(!dialog) { app.exit(12); return; }
        auto *files=dialog->findChild<QListWidget *>(QStringLiteral("postProcessImportFileList"));
        auto *buttons=dialog->findChild<QDialogButtonBox *>();
        auto *choice=dialog->findChild<QComboBox *>(QStringLiteral("postProcessImportMode"));
        if(!files || !buttons || !choice) { app.exit(13); return; }
        if(mode>=0) choice->setCurrentIndex(mode);
        auto *file=new QListWidgetItem(QFileInfo(filePath).fileName(),files); file->setData(Qt::UserRole,filePath);
        buttons->button(QDialogButtonBox::Ok)->click();
    };
    QObject::connect(&poll,&QTimer::timeout,&app,[&] {
        if(++ticks>600) { qCritical()<<"Import timeout"; app.exit(5); return; }
        if(phase<0) return;
        if(phase==2) {
            auto *message=window.findChild<QMessageBox *>();
            if(message) {
                message->accept();
                if(tree->topLevelItemCount()!=2) { qCritical()<<"Failed replace removed existing data"; app.exit(14); return; }
                qInfo()<<"Real import, selection, append, failed replace and VTK viewport passed"; app.exit(0);
            }
            return;
        }
        if(tree->topLevelItemCount()==0) return;
        auto *dataset=tree->topLevelItem(0);
        if(dataset->data(0,Qt::UserRole).toString()!=QStringLiteral("dataset")) return;
        if(phase==1) {
            if(tree->topLevelItemCount()!=2 || tree->topLevelItem(1)->data(0,Qt::UserRole).toString()!=QStringLiteral("dataset")) return;
            const auto id1=dataset->data(0,Qt::UserRole+3).toMap().value("sourceId");
            const auto id2=tree->topLevelItem(1)->data(0,Qt::UserRole+3).toMap().value("sourceId");
            if(id1==id2) { qCritical()<<"Append reuses dataset id"; app.exit(15); return; }
            phase=2; submit(path+QStringLiteral("-missing.cgns"),1); return;
        }
        if(dataset->childCount()!=(meshOnly?6:7)) { qCritical()<<"Unexpected region count"<<dataset->childCount(); app.exit(6); return; }
        auto *vtkWidget=viewer->findChild<QVTKOpenGLNativeWidget *>();
        auto *renderer=vtkWidget ? vtkWidget->GetRenderWindow()->GetRenderers()->GetFirstRenderer() : nullptr;
        if(!renderer) { qCritical()<<"Missing renderer"; app.exit(18); return; }
        auto *actors=renderer->GetActors();
        const auto countVisible=[actors] {
            int count=0; actors->InitTraversal();
            while(auto *actor=actors->GetNextActor()) if(actor->GetVisibility()) ++count;
            return count;
        };
        for(int i=0;i<dataset->childCount();++i) if(dataset->child(i)->checkState(0)!=Qt::Checked) { qCritical()<<"Region initially unchecked"; app.exit(19); return; }
        if(countVisible()!=dataset->childCount()) { qCritical()<<"Region actors initially hidden"<<countVisible(); app.exit(20); return; }
        auto *firstRegion=dataset->child(0);
        firstRegion->setCheckState(0,Qt::Unchecked);
        if(countVisible()!=dataset->childCount()-1) { qCritical()<<"Unchecking region did not hide actor"; app.exit(21); return; }
        tree->setCurrentItem(firstRegion);
        if(firstRegion->checkState(0)!=Qt::Unchecked || countVisible()!=dataset->childCount()-1) { qCritical()<<"Selecting a hidden region changed visibility"; app.exit(25); return; }
        dataset->setCheckState(0,Qt::Unchecked);
        if(countVisible()!=0) { qCritical()<<"Unchecking dataset did not hide actors"; app.exit(22); return; }
        dataset->setCheckState(0,Qt::Checked);
        if(countVisible()!=dataset->childCount()-1 || firstRegion->checkState(0)!=Qt::Unchecked) { qCritical()<<"Dataset toggle did not preserve region state"; app.exit(23); return; }
        firstRegion->setCheckState(0,Qt::Checked);
        if(countVisible()!=dataset->childCount()) { qCritical()<<"Checking region did not restore actor"; app.exit(24); return; }
        QTreeWidgetItem *zone=nullptr;
        for(int i=0;i<dataset->childCount();++i) if(dataset->child(i)->text(0).contains(QStringLiteral("wing_up"))) zone=dataset->child(i);
        if(!zone) { qCritical()<<"Wing region missing"; app.exit(9); return; }
        if(meshOnly) {
            if(zone->childCount()!=0) { qCritical()<<"Mesh-only region has fields"; app.exit(16); return; }
            tree->setCurrentItem(zone);
            if(zone->checkState(0)!=Qt::Checked) { app.exit(17); return; }
            qInfo()<<"Mesh boundaries and empty fields passed"; app.exit(0); return;
        }
        auto *fields=zone; QTreeWidgetItem *pressure=nullptr;
        for(int i=0;i<fields->childCount();++i) if(fields->child(i)->data(0,Qt::UserRole+3).toMap().value("field")==QStringLiteral("p_dim")) pressure=fields->child(i);
        if(!pressure || pressure->data(0,Qt::UserRole+3).toMap().value("association")!=QStringLiteral("point")) { qCritical()<<"Pressure field missing"; app.exit(7); return; }
        tree->setCurrentItem(pressure);
        if(dataset->child(0)->checkState(0)!=Qt::Checked || zone->checkState(0)!=Qt::Checked || countVisible()!=dataset->childCount()) { qCritical()<<"Selecting a field changed region visibility"; app.exit(10); return; }
        for(int i=0;i<dataset->childCount();++i) if(dataset->child(i)!=zone) dataset->child(i)->setCheckState(0,Qt::Unchecked);
        phase=-1;
        QTimer::singleShot(800,&app,[&] {
            const QString image=qEnvironmentVariable("AEROSIM_CGNS_CAPTURE");
            if(!image.isEmpty() && !window.grab().save(image)) { app.exit(8); return; }
            phase=1; submit(path,0);
        });
    });
    poll.start(100);
    QTimer::singleShot(500,&window,[&] { submit(path,-1); });
    return app.exec();
}
