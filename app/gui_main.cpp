#include "ApplicationController.h"
#include "MockTaskService.h"
#include "MockWorkflowService.h"
#include "MockWindow.h"

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
    window.bindServices({controller.workflow(), controller.tasks(), nullptr, nullptr, nullptr, nullptr});
    auto *placeholder = new QLabel(QStringLiteral("VTK viewport placeholder\nFormal C++ viewer will replace this"));
    placeholder->setAlignment(Qt::AlignCenter);
    if (!window.insertWidget(aerosim::WidgetSlot::Viewport, placeholder)) {
        delete placeholder;
        QTextStream(stderr) << "Failed to insert placeholder viewport\n";
        return 4;
    }
    window.show();

    const bool smoke = app.arguments().contains(QStringLiteral("--smoke"));
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
