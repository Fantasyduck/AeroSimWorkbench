#include "MockWorkflowService.h"

#include <QUuid>

namespace aerosim {

WorkflowView MockWorkflowService::view(const ScopeRef &scope) const
{
    WorkflowView result;
    result.scope = scope;
    result.viewRevision = viewRevision_;
    result.steps.append({QStringLiteral("mock.step"), scope.caseId,
                         StepState::Pending, QStringLiteral("Mock: no workflow rules"), QString()});
    result.nextSuggestion = QStringLiteral("Placeholder only: G will supply WorkflowManager");
    return result;
}

ValidationResult MockWorkflowService::requestRun(const RunRequest &request)
{
    TaskCommand command;
    command.commandId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    command.caseId = request.scope.caseId;
    command.inputSnapshotId = request.inputSnapshot.snapshotId;
    command.batchPlanId = request.batchPlanId;
    command.batchPlanVersion = request.batchPlanVersion;
    command.type = TaskCommandType::Start;
    emit taskCommandRequested(command);
    return {true, {}}; // Mock acceptance is not real validation or execution.
}

void MockWorkflowService::onTaskEvent(const TaskEvent &event)
{
    ++viewRevision_;
    emit mockEventObserved(event);
    emit workflowViewChanged(view({QStringLiteral("mock-project"), event.taskId, {}}));
}

} // namespace aerosim
