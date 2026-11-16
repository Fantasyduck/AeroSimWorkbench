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
#include <QDebug>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkActorCollection.h>
int main(int argc, char **argv) {
    if (argc!=3) return 2;
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc,argv);
    aerosim::PostProcessWorkbenchWindow window;
    auto *viewer=new aerosim::visualization::VtkSceneViewer;
    if (!window.insertWidget(aerosim::WidgetSlot::Viewport,viewer)) return 3;
    viewer->bindWorkbench(&window); window.show();
    auto *tree=window.findChild<QTreeWidget *>(QStringLiteral("postProcessDatasetTree"));
    if (!tree) return 4;
    const auto submit=[&](const QString &path) {
        auto *action=window.findChild<QAction *>(QStringLiteral("postProcessImportAction"));
        if (!action) return false; action->trigger();
        auto *dialog=window.findChild<QDialog *>(QStringLiteral("postProcessImportDataDialog"));
        if (!dialog) return false;
        auto *files=dialog->findChild<QListWidget *>(QStringLiteral("postProcessImportFileList"));
        auto *buttons=dialog->findChild<QDialogButtonBox *>();
        if (!files || !buttons) return false;
        auto *item=new QListWidgetItem(QFileInfo(path).fileName(),files); item->setData(Qt::UserRole,path);
        buttons->button(QDialogButtonBox::Ok)->click(); return true;
    };
    int phase=0, ticks=0; QTimer timer;
    QObject::connect(&timer,&QTimer::timeout,&app,[&] {
        if (++ticks>3600) { qCritical()<<"Import timeout"; app.exit(5); return; }
        if (tree->topLevelItemCount()<=phase) return;
        auto *dataset=tree->topLevelItem(phase);
        if (dataset->data(0,Qt::UserRole).toString()!=QStringLiteral("dataset")) return;
        if (!dataset->childCount() || !dataset->child(0)->childCount()) { qCritical()<<"Missing regions or fields"; app.exit(6); return; }
        auto *widget=viewer->findChild<QVTKOpenGLNativeWidget *>();
        auto *renderer=widget ? widget->GetRenderWindow()->GetRenderers()->GetFirstRenderer() : nullptr;
        if (!renderer || renderer->GetActors()->GetNumberOfItems()<phase+1) { qCritical()<<"Missing rendered actors"; app.exit(7); return; }
        if (phase==0) { ++phase; if (!submit(QString::fromLocal8Bit(argv[2]))) app.exit(8); }
        else { qInfo()<<"VTK and Tecplot workbench imports passed"; app.exit(0); }
    });
    if (!submit(QString::fromLocal8Bit(argv[1]))) return 9;
    timer.start(50); return app.exec();
}
