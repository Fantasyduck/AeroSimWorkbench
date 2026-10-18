#include "application/ApplicationController.h"
#include "infrastructure/demo/PostprocessDemoLauncher.h"
#include "mock/MockTaskService.h"
#include "mock/MockWorkflowService.h"
#include "ui_mock/MockWindow.h"

#include <QApplication>
#include <QLabel>
#include <QTextStream>
#include <QTimer>

#include <memory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    aerosim::ApplicationController controller(
        std::unique_ptr<aerosim::IWorkflowService>(new aerosim::MockWorkflowService),
        std::unique_ptr<aerosim::ITaskService>(new aerosim::MockTaskService));
    if (!controller.initialize()) {
        QTextStream(stderr) << "Failed to initialize application services\n";
        return 3;
    }

    aerosim::MockWindow window;
    aerosim::PostprocessDemoLauncher postprocessDemo;
    window.bindServices({controller.workflow(), controller.tasks(), nullptr, nullptr, nullptr, nullptr});
    QObject::connect(&window, &aerosim::MockWindow::postprocessDemoRequested,
                     &postprocessDemo, [&postprocessDemo] { postprocessDemo.launch(); });
    QObject::connect(&postprocessDemo, &aerosim::PostprocessDemoLauncher::statusMessage,
                     &window, &aerosim::MockWindow::appendMessage);
    auto *placeholder = new QLabel(QStringLiteral("VTK viewport placeholder\nC will supply the real viewer"));
    placeholder->setAlignment(Qt::AlignCenter);
    if (!window.insertWidget(aerosim::WidgetSlot::Viewport, placeholder)) {
        delete placeholder;
        QTextStream(stderr) << "Failed to insert placeholder viewport\n";
        return 4;
    }
    window.show();

    const bool smoke = app.arguments().contains(QStringLiteral("--smoke"));
    const bool postprocessSmoke = app.arguments().contains(QStringLiteral("--postprocess-smoke"));
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
    if (postprocessSmoke) {
        QObject::connect(&postprocessDemo, &aerosim::PostprocessDemoLauncher::demoFinished,
                         &app, [&app](int code, QProcess::ExitStatus status) {
            app.exit(status == QProcess::NormalExit ? code : 8);
        });
        QTimer::singleShot(0, &app, [&postprocessDemo, &app] {
            if (!postprocessDemo.launch(true))
                app.exit(9);
        });
        QTimer::singleShot(120000, &app, [&app] { app.exit(10); });
    }
    const int code = app.exec();
    controller.shutdown();
    return smoke && !smokePassed ? 7 : code;
}
