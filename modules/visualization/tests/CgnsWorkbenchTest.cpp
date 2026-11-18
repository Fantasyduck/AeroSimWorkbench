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
#include <QCheckBox>
#include <QSpinBox>
#include <QMessageBox>
#include <QToolButton>
#include <QTemporaryDir>
#include <QImage>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkDataSetMapper.h>
#include <vtkProperty.h>
#include <vtkLookupTable.h>
#include <cmath>
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
    QObject::connect(&window, &aerosim::PostProcessWorkbenchWindow::viewRequested, viewer, [viewer](const QString &view) {
        if (view == QStringLiteral("reset")) viewer->resetCamera();
        else viewer->setCameraOrientation(view);
    });
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
        // Apply a dataset style through the real dialog and verify every actor,
        // the child drafts, and subsequent individual overrides.
        auto *datasetStyleAction=window.findChild<QAction *>(QStringLiteral("postProcessObjectStyleAction"));
        const auto applyStyle=[&](QTreeWidgetItem *item,int representation,int opacity,int color,bool visible) {
            tree->setCurrentItem(item); if(!datasetStyleAction || !datasetStyleAction->isEnabled()) return false;
            datasetStyleAction->trigger();
            auto *dialog=window.findChild<QDialog *>(QStringLiteral("postProcessObjectStyleDialog"));
            auto *rep=dialog ? dialog->findChild<QComboBox *>(QStringLiteral("objectStyleRepresentation")) : nullptr;
            auto *opacityBox=dialog ? dialog->findChild<QSpinBox *>(QStringLiteral("objectStyleOpacity")) : nullptr;
            auto *colorBox=dialog ? dialog->findChild<QComboBox *>(QStringLiteral("objectStyleColor")) : nullptr;
            auto *visibility=dialog ? dialog->findChild<QCheckBox *>(QStringLiteral("objectStyleVisible")) : nullptr;
            auto *buttons=dialog ? dialog->findChild<QDialogButtonBox *>() : nullptr;
            if(!rep || !opacityBox || !colorBox || !visibility || !buttons) return false;
            rep->setCurrentIndex(representation); opacityBox->setValue(opacity); colorBox->setCurrentIndex(color); visibility->setChecked(visible);
            buttons->button(QDialogButtonBox::Ok)->click(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); return true;
        };
        if(!applyStyle(dataset,1,40,1,true)) { qCritical()<<"Dataset style dialog failed"; app.exit(50); return; }
        for(int i=0;i<dataset->childCount();++i) {
            auto *a=vtkActor::SafeDownCast(actors->GetItemAsObject(i));
            if(!a || a->GetProperty()->GetRepresentation()!=VTK_WIREFRAME || std::abs(a->GetProperty()->GetOpacity()-.4)>1e-9 || std::abs(a->GetProperty()->GetColor()[2]-.78)>1e-9) { qCritical()<<"Dataset style missed region"<<i; app.exit(51); return; }
        }
        if(!applyStyle(firstRegion,3,65,0,true)) { app.exit(52); return; }
        auto *firstActor=vtkActor::SafeDownCast(actors->GetItemAsObject(0));
        auto *otherActor=vtkActor::SafeDownCast(actors->GetItemAsObject(1));
        if(!firstActor || !otherActor || firstActor->GetProperty()->GetRepresentation()!=VTK_POINTS || otherActor->GetProperty()->GetRepresentation()!=VTK_WIREFRAME) { qCritical()<<"Individual override affected other regions"; app.exit(53); return; }
        if(!applyStyle(dataset,0,100,0,false) || countVisible()!=0) { qCritical()<<"Dataset style did not hide all regions"; app.exit(54); return; }
        if(!applyStyle(dataset,0,100,0,true) || countVisible()!=dataset->childCount()) { qCritical()<<"Dataset style did not show all regions"; app.exit(55); return; }
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
        double before[3]; renderer->GetActiveCamera()->GetPosition(before);
        tree->setCurrentItem(pressure);
        double after[3]; renderer->GetActiveCamera()->GetPosition(after);
        for (int k=0;k<3;++k) if (std::abs(before[k]-after[k])>1e-9) { qCritical()<<"Selection changed camera"; app.exit(30); return; }
        const auto context=pressure->data(0,Qt::UserRole+3).toMap();
        const QString datasetId=context.value("sourceId").toString(), regionId=context.value("regionId").toString();
        const int regionIndex=dataset->indexOfChild(zone);
        auto *actor=vtkActor::SafeDownCast(actors->GetItemAsObject(regionIndex));
        auto *mapper=actor ? vtkDataSetMapper::SafeDownCast(actor->GetMapper()) : nullptr;
        if(!mapper || !mapper->GetScalarVisibility()) { qCritical()<<"Field coloring unavailable"; app.exit(31); return; }
        emit window.fieldRangeChanged(datasetId,regionId,QStringLiteral("p_dim"),QStringLiteral("point"),false,100.,200.);
        const double *range=mapper->GetLookupTable()->GetRange();
        if(std::abs(range[0]-100.)>1e-9 || std::abs(range[1]-200.)>1e-9) { qCritical()<<"Manual range not applied"; app.exit(32); return; }
        emit window.regionStyleChanged(datasetId,regionId,QVariantMap{{"opacity",35},{"representation",2},{"color",0}});
        if(std::abs(actor->GetProperty()->GetOpacity()-.35)>1e-9 || !actor->GetProperty()->GetEdgeVisibility()) { qCritical()<<"Style not applied"; app.exit(33); return; }
        zone->setCheckState(0,Qt::Unchecked); zone->setCheckState(0,Qt::Checked);
        range=mapper->GetLookupTable()->GetRange();
        if(!mapper->GetScalarVisibility() || std::abs(range[0]-100.)>1e-9 || std::abs(actor->GetProperty()->GetOpacity()-.35)>1e-9) { qCritical()<<"Toggle lost display state"; app.exit(34); return; }
        if(dataset->child(0)->checkState(0)!=Qt::Checked || zone->checkState(0)!=Qt::Checked || countVisible()!=dataset->childCount()) { qCritical()<<"Selecting a field changed region visibility"; app.exit(10); return; }
        // Exercise the actual style dialog, not only the rendering signal.
        for(int i=0;i<dataset->childCount();++i) if(dataset->child(i)!=zone) dataset->child(i)->setCheckState(0,Qt::Unchecked);
        tree->setCurrentItem(zone);
        emit window.viewRequested(QStringLiteral("focusSelected"));
        QImage modeImages[4];
        auto *styleAction=window.findChild<QAction *>(QStringLiteral("postProcessObjectStyleAction"));
        if(!styleAction || !styleAction->isEnabled()) { qCritical()<<"Style action unavailable"; app.exit(36); return; }
        for(int mode=0;mode<4;++mode) {
            styleAction->trigger();
            auto *dialog=window.findChild<QDialog *>(QStringLiteral("postProcessObjectStyleDialog"));
            auto *choices=dialog ? dialog->findChild<QComboBox *>(QStringLiteral("objectStyleRepresentation")) : nullptr;
            auto *buttons=dialog ? dialog->findChild<QDialogButtonBox *>() : nullptr;
            if(!choices || !buttons) { qCritical()<<"Style dialog missing"; app.exit(37); return; }
            choices->setCurrentIndex(mode);
            buttons->button(QDialogButtonBox::Ok)->click();
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
            const int expected=mode==1 ? VTK_WIREFRAME : mode==3 ? VTK_POINTS : VTK_SURFACE;
            if(actor->GetProperty()->GetRepresentation()!=expected || bool(actor->GetProperty()->GetEdgeVisibility())!=(mode==2)) {
                qCritical()<<"Style representation not applied"<<mode; app.exit(38); return;
            }
            emit window.viewRequested(QStringLiteral("focusSelected"));
            modeImages[mode] = vtkWidget->grabFramebuffer();
            if(modeImages[mode].isNull()) { qCritical()<<"Empty rendered image"; app.exit(40); return; }
            if(actor->GetProperty()->GetRepresentation()!=expected || bool(actor->GetProperty()->GetEdgeVisibility())!=(mode==2)) {
                qCritical()<<"Focus changed representation"<<mode; app.exit(39); return;
            }
        }
        if(modeImages[0]==modeImages[1] || modeImages[0]==modeImages[2] || modeImages[0]==modeImages[3]) {
            qCritical()<<"Different display modes produced identical images"; app.exit(41); return;
        }
        const char *viewActions[] = {"postProcessFrontViewAction", "postProcessBackViewAction", "postProcessRightViewAction", "postProcessLeftViewAction", "postProcessTopViewAction", "postProcessBottomViewAction"};
        const int axes[] = {2, 2, 0, 0, 1, 1};
        for (int i=0;i<6;++i) {
            auto *button=window.findChild<QToolButton *>(QStringLiteral("viewToolbar_")+QString::fromLatin1(viewActions[i]));
            if(!button || button->icon().isNull()) { qCritical()<<"View shortcut missing icon"<<viewActions[i]; app.exit(42); return; }
            button->click();
            auto *camera=renderer->GetActiveCamera(); const double *position=camera->GetPosition(); const double *focal=camera->GetFocalPoint();
            const double delta=position[axes[i]]-focal[axes[i]];
            if ((i%2==0 && delta<=0) || (i%2==1 && delta>=0)) { qCritical()<<"Wrong standard view"<<viewActions[i]; app.exit(43); return; }
        }
        QTemporaryDir output;
        const QString imagePath = output.filePath(QStringLiteral("view.png"));
        emit window.exportImageRequested(imagePath, 640, 480);
        const QImage exported(imagePath);
        if (exported.isNull() || exported.size() != QSize(640, 480)) { qCritical()<<"Image export failed"; app.exit(35); return; }
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
