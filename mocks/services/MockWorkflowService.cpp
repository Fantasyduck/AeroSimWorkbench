#include "MockWorkflowService.h"

namespace aerosim {

MockWorkflowService::MockWorkflowService(QObject *parent)
    : IWorkflowService(parent), engine_(nullptr)
{
    connect(&engine_, &IWorkflowService::taskCommandRequested,
            this, &IWorkflowService::taskCommandRequested);
    connect(&engine_, &IWorkflowService::workflowViewV2Changed,
            this, &IWorkflowService::workflowViewV2Changed);
}

AccessResult<WorkflowViewV2> MockWorkflowService::viewV2(const ScopeRef &scope) const
{
    return engine_.viewV2(scope);
}

void MockWorkflowService::ensureBootstrap(const RunRequest &request)
{
    if (bootstrapped_)
        return;
    workflow::WorkflowDefinition definition;
    definition.definitionVersion = QStringLiteral("mock-workflow-v2");
    workflow::WorkflowStepDefinition step;
    step.stepId = QStringLiteral("mock.step");
    step.summary = QStringLiteral("Mock: assembly validation only");
    step.targetPageId = QStringLiteral("parameters");
    step.applicabilityRuleId = QStringLiteral("mock.always");
    definition.steps.append(step);
    workflow::WorkflowActionRule run;
    run.actionId = QStringLiteral("run");
    run.requiredStepIds.append(step.stepId);
    run.requiresCurrentValidation = true;
    definition.actions.append(run);
    engine_.setDefinition(definition);
    workflow::ValidationInputStamp stamp;
    stamp.scope = request.scope;
    stamp.definitionVersion = definition.definitionVersion;
    stamp.parameter = request.inputSnapshot;
    stamp.parameter.scope = request.scope;
    if (stamp.parameter.revision == 0)
        stamp.parameter.revision = 1;
    engine_.setCurrentInput(request.scope, step.stepId, stamp);
    engine_.saveEditing(request.scope, step.stepId);
    engine_.acceptValidation(request.scope, step.stepId,
        workflow::ValidationOutcome::Passed, stamp);
    bootstrapped_ = true;
}

ValidationResult MockWorkflowService::requestRun(const RunRequest &request)
{
    ensureBootstrap(request);
    RunRequest normalized = request;
    normalized.inputSnapshot.scope = request.scope;
    if (normalized.inputSnapshot.revision == 0)
        normalized.inputSnapshot.revision = 1;
    return engine_.requestRun(normalized);
}

void MockWorkflowService::onTaskEvent(const TaskEvent &event)
{
    emit mockEventObserved(event);
    engine_.onTaskEvent(event);
}

} // namespace aerosim
