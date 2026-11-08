#include "WorkflowTestSupport.h"

#include "aerosim/workflow/ActionAuthorizer.h"
#include "aerosim/workflow/WorkflowService.h"

#include <QCoreApplication>

namespace {

aerosim::ScopeRef scope()
{
    aerosim::ScopeRef result;
    result.projectId = QStringLiteral("project");
    result.caseId = QStringLiteral("case");
    result.objectId = QStringLiteral("state");
    return result;
}

aerosim::workflow::ValidationInputStamp stamp(quint64 revision)
{
    aerosim::workflow::ValidationInputStamp result;
    result.scope = scope();
    result.definitionVersion = QStringLiteral("v2");
    result.parameter.snapshotId = QStringLiteral("parameters");
    result.parameter.scope = result.scope;
    result.parameter.revision = revision;
    return result;
}

aerosim::workflow::WorkflowActionRule rule(const QString &id, bool current)
{
    aerosim::workflow::WorkflowActionRule result;
    result.actionId = id;
    result.targetPageId = QStringLiteral("parameters-page");
    result.requiredStepIds.append(QStringLiteral("configured"));
    result.requiresCurrentValidation = current;
    return result;
}

aerosim::workflow::WorkflowDefinition definition()
{
    aerosim::workflow::WorkflowDefinition result;
    result.definitionVersion = QStringLiteral("v2");
    aerosim::workflow::WorkflowStepDefinition step;
    step.stepId = QStringLiteral("configured");
    step.targetPageId = QStringLiteral("parameters-page");
    step.applicabilityRuleId = QStringLiteral("always");
    result.steps.append(step);
    result.actions.append(rule(QStringLiteral("run"), true));
    result.actions.append(rule(QStringLiteral("mock-batch-submit"), true));
    result.actions.append(rule(QStringLiteral("browse"), false));
    result.actions.append(rule(QStringLiteral("navigate"), false));
    result.actions.append(rule(QStringLiteral("edit"), false));
    result.actions.append(rule(QStringLiteral("save"), false));
    result.actions.append(rule(QStringLiteral("revalidate"), false));
    result.actions.append(rule(QStringLiteral("mock-plan-edit"), false));
    return result;
}

bool hasCode(const QVector<aerosim::ValidationIssue> &issues,
             const QString &code)
{
    for (const aerosim::ValidationIssue &issue : issues) {
        if (issue.code == code && !issue.message.isEmpty()
            && !issue.targetPageId.isEmpty()) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application);
    aerosim_workflow_test::TestContext context;

    aerosim::WorkflowViewV2 view;
    view.scope = scope();
    aerosim::workflow::WorkflowStepEvaluationState state;
    state.step.stepId = QStringLiteral("configured");
    state.step.scope = view.scope;
    state.step.applicable = true;
    state.step.completion = aerosim::CompletionState::Completed;
    state.step.validation = aerosim::ValidationState::Valid;
    state.step.targetPageId = QStringLiteral("parameters-page");
    state.inputVersionMatches = true;
    view.steps.append(state.step);
    aerosim::workflow::WorkflowStepEvaluationStateMap states;
    states.insert(state.step.stepId, state);

    aerosim::workflow::ActionAuthorization authorization =
        aerosim::workflow::authorizeAction(rule(QStringLiteral("run"), true),
                                           view, states);
    AEROSIM_WORKFLOW_CHECK(context, authorization.allowed());

    states[QStringLiteral("configured")].step.applicable = false;
    authorization = aerosim::workflow::authorizeAction(
        rule(QStringLiteral("run"), true), view, states);
    AEROSIM_WORKFLOW_CHECK(context,
        hasCode(authorization.issues,
                QStringLiteral("workflow.action.not_applicable")));
    states[QStringLiteral("configured")].step.applicable = true;
    states[QStringLiteral("configured")].step.completion =
        aerosim::CompletionState::InProgress;
    authorization = aerosim::workflow::authorizeAction(
        rule(QStringLiteral("run"), true), view, states);
    AEROSIM_WORKFLOW_CHECK(context,
        hasCode(authorization.issues,
                QStringLiteral("workflow.action.not_completed")));
    states[QStringLiteral("configured")].step.completion =
        aerosim::CompletionState::Completed;
    states[QStringLiteral("configured")].step.validation =
        aerosim::ValidationState::Stale;
    authorization = aerosim::workflow::authorizeAction(
        rule(QStringLiteral("run"), true), view, states);
    AEROSIM_WORKFLOW_CHECK(context,
        hasCode(authorization.issues,
                QStringLiteral("workflow.action.validation_required")));

    // Repair/view actions remain enabled while stale.
    authorization = aerosim::workflow::authorizeAction(
        rule(QStringLiteral("edit"), false), view, states);
    AEROSIM_WORKFLOW_CHECK(context, authorization.allowed());

    states[QStringLiteral("configured")].step.validation =
        aerosim::ValidationState::Valid;
    aerosim::DependencyBlockerView blocker;
    blocker.code = QStringLiteral("upstream.invalid");
    blocker.message = QStringLiteral("upstream is invalid");
    blocker.targetPageId = QStringLiteral("upstream-page");
    states[QStringLiteral("configured")].step.blockers.append(blocker);
    authorization = aerosim::workflow::authorizeAction(
        rule(QStringLiteral("run"), true), view, states);
    AEROSIM_WORKFLOW_CHECK(context,
        hasCode(authorization.issues,
                QStringLiteral("workflow.action.dependency_blocked")));
    states[QStringLiteral("configured")].step.blockers.clear();
    states[QStringLiteral("configured")].inputVersionMatches = false;
    authorization = aerosim::workflow::authorizeAction(
        rule(QStringLiteral("run"), true), view, states);
    AEROSIM_WORKFLOW_CHECK(context,
        hasCode(authorization.issues,
                QStringLiteral("workflow.action.input_changed")));

    aerosim::workflow::WorkflowService service;
    AEROSIM_WORKFLOW_CHECK(context,
        service.setDefinition(definition()).ok());
    const aerosim::workflow::ValidationInputStamp revision1 = stamp(1);
    AEROSIM_WORKFLOW_CHECK(context,
        service.setCurrentInput(scope(), QStringLiteral("configured"),
                                revision1).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        service.saveEditing(scope(), QStringLiteral("configured")).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        service.acceptValidation(scope(), QStringLiteral("configured"),
            aerosim::workflow::ValidationOutcome::Passed, revision1).ok());

    int commandCount = 0;
    int startCount = 0;
    int stopOrCancelCount = 0;
    QObject::connect(&service, &aerosim::IWorkflowService::taskCommandRequested,
        [&commandCount, &startCount, &stopOrCancelCount](
            const aerosim::TaskCommand &command) {
            ++commandCount;
            if (command.type == aerosim::TaskCommandType::Start)
                ++startCount;
            if (command.type == aerosim::TaskCommandType::Stop
                || command.type == aerosim::TaskCommandType::Cancel) {
                ++stopOrCancelCount;
            }
        });

    aerosim::RunRequest request;
    request.scope = scope();
    request.inputSnapshot = revision1.parameter;
    AEROSIM_WORKFLOW_CHECK(context, service.requestRun(request).allowed);
    AEROSIM_WORKFLOW_CHECK(context, commandCount == 1);
    AEROSIM_WORKFLOW_CHECK(context, startCount == 1);

    const aerosim::workflow::ValidationInputStamp revision2 = stamp(2);
    AEROSIM_WORKFLOW_CHECK(context,
        service.setCurrentInput(scope(), QStringLiteral("configured"),
                                revision2).ok());
    request.inputSnapshot = revision2.parameter;
    const aerosim::ValidationResult denied = service.requestRun(request);
    AEROSIM_WORKFLOW_CHECK(context, !denied.allowed);
    AEROSIM_WORKFLOW_CHECK(context,
        hasCode(denied.issues,
                QStringLiteral("workflow.action.validation_required")));
    AEROSIM_WORKFLOW_CHECK(context, commandCount == 1);
    AEROSIM_WORKFLOW_CHECK(context, stopOrCancelCount == 0);

    const aerosim::WorkflowViewV2 staleView = service.viewV2(scope()).value();
    for (const aerosim::WorkflowActionView &action : staleView.actions) {
        if (action.actionId == QStringLiteral("run")
            || action.actionId == QStringLiteral("mock-batch-submit"))
            AEROSIM_WORKFLOW_CHECK(context, !action.enabled);
        else
            AEROSIM_WORKFLOW_CHECK(context, action.enabled);
    }

    return context.result();
}
