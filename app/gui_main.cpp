#include "ApplicationController.h"
#include "MockTaskService.h"
#include "MockWorkflowService.h"
#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/StartupLandingPage.h"
#ifdef AEROSIM_HAS_MONITOR_CURVES
#include "aerosim/ui/monitor/ComputeMonitorPage.h"
#include "aerosim/ui/monitor/MonitorSampleCurveWidget.h"
#endif
#ifdef AEROSIM_HAS_RESULT_SOURCE
#include "aerosim/ui/intelligent/MonitorResultDataSource.h"
#include "aerosim/ui/intelligent/ResultDataTablePage.h"
#include "aerosim/ui/intelligent/ResultSummaryTablePage.h"
#endif
#ifdef AEROSIM_HAS_RESULT_CURVES
#include "aerosim/ui/intelligent/QwtResultCurveView.h"
#include "aerosim/ui/intelligent/ResultDataTablePage.h"
#endif
#include <QPixmap>

#ifdef AEROSIM_HAS_VTK_VIEWER
#include "aerosim/visualization/scene/VtkSceneViewer.h"
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#endif

#include <QApplication>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QTimer>

#include <memory>

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#ifdef AEROSIM_HAS_VTK_VIEWER
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
#endif
    QApplication app(argc, argv);
    aerosim::ApplicationController controller(
        std::unique_ptr<aerosim::IWorkflowService>(new aerosim::MockWorkflowService),
        std::unique_ptr<aerosim::ITaskService>(new aerosim::MockTaskService));
    if (!controller.initialize()) {
        QTextStream(stderr) << "Failed to initialize application services\n";
        return 3;
    }

    const bool smoke = app.arguments().contains(QStringLiteral("--smoke"));
#ifndef AEROSIM_POSTPROCESS_STANDALONE
#ifdef AEROSIM_HAS_RESULT_SOURCE
    aerosim::MonitorResultDataSource resultReader;
#endif
    aerosim::MainWindow window;
    aerosim::StartupLandingPage landing;
#endif
    aerosim::PostProcessWorkbenchWindow postprocess;
    const aerosim::ServiceBindings bindings = {
        controller.workflow(), controller.tasks(), nullptr, nullptr, nullptr, nullptr};
#ifndef AEROSIM_POSTPROCESS_STANDALONE
    window.bindServices(bindings);
#ifdef AEROSIM_HAS_MONITOR_CURVES
    // The curve slot is filled here, in the assembly layer, and not inside the
    // window: the page owns the slot and accepts exactly one widget, so a test
    // that injects its own double would be locked out by a self-installed one.
    {
        auto *curves = new aerosim::MonitorSampleCurveWidget;
        if (!window.monitorPage()->insertCurveWidget(curves)) {
            delete curves;
            QTextStream(stderr) << "Failed to insert monitor curve widget\n";
        }
    }
#endif
#ifdef AEROSIM_HAS_RESULT_SOURCE
    // Same reasoning as the curve slot above: the result pages take a reader and
    // a curve view through their abstract ports, and only this executable has the
    // monitor reader and the chart stack linked in. A build without the
    // visualization module leaves both slots empty and the pages say so instead
    // of pretending they can open a file.
    {
        auto *reader = &resultReader;
        window.resultDataTablePage()->setDataSource(reader);
        window.resultSummaryTablePage()->setDataSource(reader);
    }
#endif
#ifdef AEROSIM_HAS_RESULT_CURVES
    if (!window.resultDataTablePage()->setCurveView(new aerosim::QwtResultCurveView))
        QTextStream(stderr) << "Failed to insert result curve view\n";
#endif
    if (app.arguments().contains(QStringLiteral("--monitor")))
        window.showComputeMonitor();
#endif
    postprocess.bindServices(bindings);
#ifndef AEROSIM_POSTPROCESS_STANDALONE
    QObject::connect(&window, &aerosim::MainWindow::openPostProcessRequested, &postprocess, [&postprocess] {
        postprocess.show();
        postprocess.raise();
        postprocess.activateWindow();
    });
    QObject::connect(&landing, &aerosim::StartupLandingPage::enterWorkbenchRequested,
                     &window, [&landing, &window](const QString &) {
        landing.setReturnAvailable(true);
        landing.hide();
        window.show();
        window.raise();
        window.activateWindow();
    });
    QObject::connect(&landing, &aerosim::StartupLandingPage::returnToWorkbenchRequested,
                     &window, [&landing, &window] {
        landing.hide();
        window.show();
        window.raise();
        window.activateWindow();
    });
    QObject::connect(&window, &aerosim::MainWindow::showStartupRequested,
                     &landing, [&landing, &window] {
        window.hide();
        landing.setReturnAvailable(true);
        landing.show();
        landing.raise();
        landing.activateWindow();
    });
#endif
#ifdef AEROSIM_HAS_VTK_VIEWER
    // The workbench remains usable without a native OpenGL surface.  This is
    // also the safe default on remote desktop sessions where VTK's native
    // widget may not be backed by the expected graphics driver.
    const bool enableVtkViewer = qEnvironmentVariableIntValue("AEROSIM_ENABLE_VTK_VIEWER") == 1;
    if (!smoke && enableVtkViewer) {
        auto *viewer = new aerosim::visualization::VtkSceneViewer;
        if (!postprocess.insertWidget(aerosim::WidgetSlot::Viewport, viewer)) {
            delete viewer;
            QTextStream(stderr) << "Failed to insert VTK viewer\n";
            return 4;
        }
        QObject::connect(&postprocess, &aerosim::PostProcessWorkbenchWindow::viewRequested, viewer,
                         [viewer](const QString &orientation) {
            if (orientation == QStringLiteral("reset"))
                viewer->resetCamera();
            else
                viewer->setCameraOrientation(orientation);
        });
    }
#endif
#ifdef AEROSIM_POSTPROCESS_STANDALONE
    QWidget *initialWindow = &postprocess;
#else
    QWidget *initialWindow = app.arguments().contains(QStringLiteral("--postprocess"))
        ? static_cast<QWidget *>(&postprocess)
        : (app.arguments().contains(QStringLiteral("--workbench"))
           || app.arguments().contains(QStringLiteral("--monitor")))
            ? static_cast<QWidget *>(&window) : static_cast<QWidget *>(&landing);
#endif
    initialWindow->show();

    const int screenshotIndex = app.arguments().indexOf(QStringLiteral("--screenshot"));
    if (screenshotIndex >= 0) {
        if (screenshotIndex + 1 >= app.arguments().size()) return 2;
        const QString path = app.arguments().at(screenshotIndex + 1);
        QTimer::singleShot(500, initialWindow, [initialWindow, &app, path] {
            app.exit(initialWindow->grab().save(path) ? 0 : 2);
        });
    }

    bool smokePassed = false;
    if (smoke) {
        QObject::connect(controller.tasks(), &aerosim::ITaskService::taskEventOccurred, &app,
                         [&app, &smokePassed](const aerosim::TaskEvent &event) {
            if (event.type == aerosim::TaskEventType::Finished) {
                smokePassed = true;
                QTextStream(stdout) << "MOCK GUI smoke passed\n";
                app.quit();
            }
        }, Qt::QueuedConnection);
        QTimer::singleShot(0, &app, [&controller] {
            aerosim::RunRequest request;
            request.scope.caseId = QStringLiteral("mock-case");
            request.inputSnapshot.snapshotId = QStringLiteral("mock-snapshot");
            controller.workflow()->requestRun(request);
        });
        QTimer::singleShot(3000, &app, &QCoreApplication::quit);
    }
    const int code = app.exec();
    controller.shutdown();
    return smoke && !smokePassed ? 7 : code;
}
