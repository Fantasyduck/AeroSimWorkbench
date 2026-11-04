#include "ApplicationController.h"
#include "MockTaskService.h"
#include "MockWorkflowService.h"
#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/StartupLandingPage.h"
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
    aerosim::MainWindow window;
    aerosim::StartupLandingPage landing;
#endif
    aerosim::PostProcessWorkbenchWindow postprocess;
    const aerosim::ServiceBindings bindings = {
        controller.workflow(), controller.tasks(), nullptr, nullptr, nullptr, nullptr};
#ifndef AEROSIM_POSTPROCESS_STANDALONE
    window.bindServices(bindings);
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
        : app.arguments().contains(QStringLiteral("--workbench"))
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
