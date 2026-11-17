#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include "aerosim/visualization/VtkSceneViewer.h"
#include <QApplication>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#include <QTimer>
#include <QTreeWidget>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QListWidget>
#include <QFileInfo>
#include <QMouseEvent>
#include <QImage>
#include <QDebug>
#include <cmath>

int main(int argc,char **argv) {
    if(argc!=2) return 2;
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc,argv);
    aerosim::PostProcessWorkbenchWindow window;
    auto *viewer=new aerosim::visualization::VtkSceneViewer;
    if(!window.insertWidget(aerosim::WidgetSlot::Viewport,viewer)) return 3;
    viewer->bindWorkbench(&window); window.resize(1400,900); window.show();
    auto *tree=window.findChild<QTreeWidget *>(QStringLiteral("postProcessDatasetTree"));
    auto *vtk=viewer->findChild<QVTKOpenGLNativeWidget *>();
    if(!tree || !vtk) return 4;
    const QString path=QString::fromLocal8Bit(argv[1]);
    QTimer::singleShot(500,&window,[&] {
        auto *action=window.findChild<QAction *>(QStringLiteral("postProcessImportAction"));
        if(!action) { app.exit(5); return; } action->trigger();
        auto *dialog=window.findChild<QDialog *>(QStringLiteral("postProcessImportDataDialog"));
        if(!dialog) { app.exit(6); return; }
        auto *files=dialog->findChild<QListWidget *>(QStringLiteral("postProcessImportFileList"));
        auto *buttons=dialog->findChild<QDialogButtonBox *>();
        if(!files || !buttons) { app.exit(7); return; }
        auto *file=new QListWidgetItem(QFileInfo(path).fileName(),files); file->setData(Qt::UserRole,path);
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    int tick=0,region=0; bool pending=false,failed=false; QTimer timer;
    QObject::connect(&timer,&QTimer::timeout,&app,[&] {
        if(++tick>400) { qCritical()<<"Timed out"; app.exit(8); return; }
        if(pending || tree->topLevelItemCount()==0) return;
        auto *dataset=tree->topLevelItem(0);
        if(dataset->data(0,Qt::UserRole).toString()!=QStringLiteral("dataset")) return;
        if(dataset->childCount()!=7) {qCritical()<<"Unexpected regions"<<dataset->childCount(); app.exit(9); return;}
        if(region>=dataset->childCount()) {app.exit(failed?10:0); return;}
        auto *item=dataset->child(region++);
        for(int i=0;i<dataset->childCount();++i) dataset->child(i)->setCheckState(0,Qt::Unchecked);
        tree->scrollToItem(item); const QRect rect=tree->visualItemRect(item);
        const QPoint point=rect.topLeft()+QPoint(9,rect.height()/2);
        QMouseEvent down(QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent up(QEvent::MouseButtonRelease,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(),&down); QApplication::sendEvent(tree->viewport(),&up);
        pending=true;
        QTimer::singleShot(600,&app,[&,item] {
            const QImage frame=vtk->grabFramebuffer().convertToFormat(QImage::Format_RGB32);
            qint64 different=0;
            for(int y=0;y<frame.height();y+=2) { const QColor bg=frame.pixelColor(frame.width()-2,y);
                for(int x=0;x<frame.width()-4;x+=2) { const QColor c=frame.pixelColor(x,y);
                    if(std::abs(c.red()-bg.red())+std::abs(c.green()-bg.green())+std::abs(c.blue()-bg.blue())>35) ++different;
                }
            }
            qInfo()<<item->text(0)<<"checked="<<(item->checkState(0)==Qt::Checked)<<"visiblePixels="<<different;
            if(item->checkState(0)!=Qt::Checked || different<100) failed=true;
            pending=false;
        });
    });
    timer.start(100); return app.exec();
}
