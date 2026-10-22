#include "ApplicationController.h"
#include "MockTaskService.h"
#include "MockWorkflowService.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

#include <memory>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream output(stdout);
    QTextStream error(stderr);
    if (app.arguments().size() != 2 || app.arguments().at(1) != QStringLiteral("--mock-once")) {
        error << "Usage: aerosim_cli --mock-once (framework demonstration only)\n";
        return 2;
    }

    aerosim::ApplicationController controller(
        std::unique_ptr<aerosim::IWorkflowService>(new aerosim::MockWorkflowService),
        std::unique_ptr<aerosim::ITaskService>(new aerosim::MockTaskService));
    if (!controller.initialize()) {
        error << "Failed to initialize application services\n";
        return 3;
    }
    if (!controller.initialize()) {
        error << "Repeated initialization failed\n";
        return 3;
    }

    int exitCode = 4;
    int eventCount = 0;
    int workflowEventCount = 0;
    bool executorRanDuringSubmission = false;
    bool submitting = false;
    QObject::connect(static_cast<aerosim::MockWorkflowService *>(controller.workflow()),
                     &aerosim::MockWorkflowService::mockEventObserved, &app,
                     [&](const aerosim::TaskEvent &) { ++workflowEventCount; });
    QObject::connect(controller.tasks(), &aerosim::ITaskService::taskEventOccurred, &app,
                     [&](const aerosim::TaskEvent &) {
        if (submitting)
            executorRanDuringSubmission = true;
    }, Qt::DirectConnection);
    QObject::connect(controller.tasks(), &aerosim::ITaskService::taskEventOccurred, &app,
                     [&](const aerosim::TaskEvent &event) {
        ++eventCount;
        output << "MOCK event: " << event.message << " [sequence=" << event.sequence << "]\n";
        output.flush();
        if (event.type == aerosim::TaskEventType::Finished) {
            const auto record = controller.tasks()->record(event.taskId);
            exitCode = (!executorRanDuringSubmission && eventCount == 2 &&
                        workflowEventCount == 2 && record.status == aerosim::TaskStatus::Finished) ? 0 : 5;
            app.quit();
        }
    }, Qt::QueuedConnection);

    QTimer::singleShot(0, &app, [&] {
        aerosim::RunRequest request;
        request.scope.projectId = QStringLiteral("mock-project");
        request.scope.caseId = QStringLiteral("mock-case");
        request.inputSnapshot.snapshotId = QStringLiteral("mock-snapshot");
        submitting = true;
        const auto result = controller.workflow()->requestRun(request);
        submitting = false;
        if (!result.allowed) {
            error << "Mock submission failed\n";
            app.exit(6);
            return;
        }
        output << "MOCK command queued; no solver will run\n";
        output.flush();
    });
    QTimer::singleShot(3000, &app, [&] { error << "Timed out waiting for MOCK event\n"; app.quit(); });
    app.exec();
    controller.shutdown();
    return exitCode;
}
