#include "WorkflowTestSupport.h"

#include "aerosim/workflow/LegacyWorkflowAdapter.h"

namespace {

aerosim::WorkflowViewV2 baseView()
{
    aerosim::WorkflowViewV2 view;
    view.scope.projectId = QStringLiteral("project");
    view.scope.caseId = QStringLiteral("case");
    view.scope.objectId = QStringLiteral("state");
    view.viewRevision = 9;
    view.nextSuggestion = QStringLiteral("next");
    aerosim::WorkflowActionView action;
    action.actionId = QStringLiteral("run");
    view.actions.append(action);
    aerosim::ValidationIssue issue;
    issue.code = QStringLiteral("issue");
    view.issues.append(issue);
    return view;
}

aerosim::WorkflowStepViewV2 step(aerosim::CompletionState completion,
                                 aerosim::ValidationState validation,
                                 bool applicable = true)
{
    aerosim::WorkflowStepViewV2 value;
    value.stepId = QStringLiteral("step");
    value.scope = baseView().scope;
    value.applicable = applicable;
    value.completion = completion;
    value.validation = validation;
    value.summary = QStringLiteral("summary");
    value.targetPageId = QStringLiteral("page");
    return value;
}

aerosim::StepState map(aerosim::WorkflowStepViewV2 value)
{
    aerosim::WorkflowViewV2 view = baseView();
    view.steps.append(value);
    return aerosim::workflow::toLegacyWorkflowView(view).steps.first().state;
}

} // namespace

int main()
{
    aerosim_workflow_test::TestContext context;
    AEROSIM_WORKFLOW_CHECK(context,
        map(step(aerosim::CompletionState::Completed,
                 aerosim::ValidationState::Valid))
            == aerosim::StepState::Complete);
    AEROSIM_WORKFLOW_CHECK(context,
        map(step(aerosim::CompletionState::Completed,
                 aerosim::ValidationState::Stale))
            == aerosim::StepState::NeedsValidation);
    AEROSIM_WORKFLOW_CHECK(context,
        map(step(aerosim::CompletionState::Completed,
                 aerosim::ValidationState::Invalid))
            == aerosim::StepState::NeedsValidation);
    AEROSIM_WORKFLOW_CHECK(context,
        map(step(aerosim::CompletionState::InProgress,
                 aerosim::ValidationState::Valid))
            == aerosim::StepState::Pending);
    AEROSIM_WORKFLOW_CHECK(context,
        map(step(aerosim::CompletionState::Completed,
                 aerosim::ValidationState::NotValidated))
            == aerosim::StepState::Pending);
    AEROSIM_WORKFLOW_CHECK(context,
        map(step(aerosim::CompletionState::Completed,
                 aerosim::ValidationState::Valid, false))
            == aerosim::StepState::Pending);

    aerosim::WorkflowStepViewV2 blocked = step(
        aerosim::CompletionState::Completed, aerosim::ValidationState::Valid);
    aerosim::DependencyBlockerView blocker;
    blocker.code = QStringLiteral("blocked");
    blocked.blockers.append(blocker);
    AEROSIM_WORKFLOW_CHECK(context,
        map(blocked) == aerosim::StepState::Blocked);

    aerosim::WorkflowViewV2 source = baseView();
    source.steps.append(step(aerosim::CompletionState::InProgress,
                             aerosim::ValidationState::Valid));
    const aerosim::WorkflowView result =
        aerosim::workflow::toLegacyWorkflowView(source);
    AEROSIM_WORKFLOW_CHECK(context, result.viewRevision == 9);
    AEROSIM_WORKFLOW_CHECK(context,
        result.scope.caseId == QStringLiteral("case"));
    AEROSIM_WORKFLOW_CHECK(context,
        result.steps.first().summary == QStringLiteral("summary"));
    AEROSIM_WORKFLOW_CHECK(context,
        result.steps.first().targetPageId == QStringLiteral("page"));
    AEROSIM_WORKFLOW_CHECK(context,
        result.actions.size() == 1 && result.issues.size() == 1
            && result.nextSuggestion == QStringLiteral("next"));
    return context.result();
}
