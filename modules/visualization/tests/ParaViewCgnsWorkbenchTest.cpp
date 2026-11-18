#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include "aerosim/visualization/VtkSceneViewer.h"
#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QImage>
#include <QSurfaceFormat>
#include <QTimer>
#include <QTreeWidget>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkDataSetMapper.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <cmath>
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    qputenv("AEROSIM_CGNS_BACKEND", "paraview");
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);
    aerosim::PostProcessWorkbenchWindow window;
    auto *viewer = new aerosim::visualization::VtkSceneViewer;
    if (!window.insertWidget(aerosim::WidgetSlot::Viewport, viewer)) return 3;
    viewer->bindWorkbench(&window); window.resize(1200, 800); window.show();
    auto *tree = window.findChild<QTreeWidget *>(QStringLiteral("postProcessDatasetTree"));
    if (!tree) return 4;
    const QString path = QString::fromLocal8Bit(argv[1]);
    QElapsedTimer importTimer;
    QTimer timeout; timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &app, [&] { std::cerr << "Import timed out\n"; app.exit(5); });
    timeout.start(qEnvironmentVariableIntValue("AEROSIM_CGNS_TEST_TIMEOUT_MS") > 0 ? qEnvironmentVariableIntValue("AEROSIM_CGNS_TEST_TIMEOUT_MS") : 30000);
    QTimer poll; QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        if (tree->topLevelItemCount() != 1) return;
        auto *dataset = tree->topLevelItem(0);
        if (dataset->data(0, Qt::UserRole).toString() != QStringLiteral("dataset")) return;
        poll.stop(); timeout.stop();
        std::cout << "import_to_tree_ms=" << importTimer.elapsed() << "\n";
        std::cout << "dataset=" << dataset->text(0).toStdString() << " regions=" << dataset->childCount() << "\n";
        if (dataset->childCount() < 1) { app.exit(6); return; }
        const QStringList onlyRegions = qEnvironmentVariable("AEROSIM_CGNS_ONLY_REGIONS").split(';', QString::SkipEmptyParts);
        QTreeWidgetItem *pressure = nullptr;
        QList<QTreeWidgetItem *> selectedFields;
        for (int i=0;i<dataset->childCount();++i) {
            auto *region=dataset->child(i);
            std::cout << "region=" << region->text(0).toStdString() << " fields=" << region->childCount() << "\n";
            const bool showRegion = onlyRegions.isEmpty() || onlyRegions.contains(region->text(0).section(" / ", -1));
            for (int j=0;j<region->childCount();++j) {
                auto *field=region->child(j); const auto context=field->data(0,Qt::UserRole+3).toMap();
                std::cout << " field=" << context.value("field").toString().toStdString() << " association=" << context.value("association").toString().toStdString() << "\n";
                if (!onlyRegions.isEmpty() && showRegion && context.value("field").toString()==QStringLiteral("Pressure")) selectedFields.append(field);
                if (i==0 && context.value("field").toString()==QStringLiteral("Pressure") && context.value("association").toString()==QStringLiteral("cell")) pressure=field;
            }
            if (!onlyRegions.isEmpty()) region->setCheckState(0, showRegion ? Qt::Checked : Qt::Unchecked);
            else if (i>0 && !qEnvironmentVariableIsSet("AEROSIM_CGNS_SHOW_ALL_REGIONS")) region->setCheckState(0,Qt::Unchecked);
        }
        if (!pressure && !qEnvironmentVariableIsSet("AEROSIM_CGNS_ALLOW_NO_PRESSURE")) { std::cerr << "Cell Pressure field missing\n"; app.exit(7); return; }
        if (onlyRegions.isEmpty() && pressure) tree->setCurrentItem(pressure);
        for (auto *field : selectedFields) tree->setCurrentItem(field);
        auto *vtk=viewer->findChild<QVTKOpenGLNativeWidget *>();
        auto *renderer=vtk ? vtk->GetRenderWindow()->GetRenderers()->GetFirstRenderer() : nullptr;
        if (!renderer) { app.exit(8); return; }
        auto *actors=renderer->GetActors(); actors->InitTraversal();
        if (!onlyRegions.isEmpty()) {
            for (int i=0; i<dataset->childCount(); ++i) { auto *a=actors->GetNextActor(); if (a && a->GetVisibility()) { const double *b=a->GetBounds(); std::cout << "bounds " << dataset->child(i)->text(0).toStdString() << " " << b[0] << " " << b[1] << " " << b[2] << " " << b[3] << " " << b[4] << " " << b[5] << "\n"; } }
            actors->InitTraversal();
        }
        auto *actor=actors->GetNextActor();
        auto *mapper=actor ? vtkDataSetMapper::SafeDownCast(actor->GetMapper()) : nullptr;
        if (!mapper || (onlyRegions.isEmpty() && pressure && (!mapper->GetScalarVisibility() || mapper->GetScalarMode()!=VTK_SCALAR_MODE_USE_CELL_FIELD_DATA))) { app.exit(9); return; }
        viewer->resetCamera();
        QTimer::singleShot(500, &app, [&app, vtk] {
            QImage image=vtk->grabFramebuffer();
            if (image.isNull()) { app.exit(10); return; }
            const QString output=qEnvironmentVariable("AEROSIM_CGNS_CAPTURE");
            if (!output.isEmpty() && !image.save(output)) { app.exit(11); return; }
            std::cout << "render=" << image.width() << "x" << image.height() << "\n";
            app.exit(0);
        });
    }); poll.start(100);
    QTimer::singleShot(300, &window, [&window, &importTimer, path] { importTimer.start(); emit window.importDataRequested(QStringList{path}, QStringLiteral("replace")); });
    return app.exec();
}
