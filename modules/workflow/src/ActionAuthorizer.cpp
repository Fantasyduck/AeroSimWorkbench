#include "aerosim/workflow/ActionAuthorizer.h"

namespace aerosim {
namespace workflow {
namespace {

ValidationIssue makeIssue(const QString &code, const QString &message,
                          const QString &targetPageId,
                          const WorkflowViewV2 &view)
{
    ValidationIssue issue;
    issue.code = code;
    issue.message = message;
    issue.scopeId = view.scope.caseId;
    issue.objectId = view.scope.objectId;
    issue.targetPageId = targetPageId;
    return issue;
}

void addIssue(ActionAuthorization &result, const WorkflowViewV2 &view,
              const QString &code, const QString &message,
              const QString &targetPageId)
{
    result.issues.append(makeIssue(code, message, targetPageId, view));
}

} // namespace

ActionAuthorization authorizeAction(
    const WorkflowActionRule &rule,
    const WorkflowViewV2 &view,
    const WorkflowStepEvaluationStateMap &states,
    quint64 *operationCount)
{
    ActionAuthorization result;
    result.action.actionId = rule.actionId;
    result.action.targetPageId = rule.targetPageId;
    result.action.scopeId = view.scope.caseId;
    result.action.objectId = view.scope.objectId;

    if (!rule.requiresCurrentValidation) {
        result.action.enabled = true;
        return result;
    }

    for (const QString &stepId : rule.requiredStepIds) {
        if (operationCount)
            ++(*operationCount);
        const auto found = states.constFind(stepId);
        if (found == states.constEnd()) {
            addIssue(result, view,
                QStringLiteral("workflow.action.step_missing"),
                QStringLiteral("required workflow step is unavailable"),
                rule.targetPageId);
            continue;
        }
        const WorkflowStepEvaluationState &state = found.value();
        const QString target = state.step.targetPageId.isEmpty()
            ? rule.targetPageId : state.step.targetPageId;
        if (!state.step.applicable) {
            addIssue(result, view,
                QStringLiteral("workflow.action.not_applicable"),
                QStringLiteral("required workflow step is not applicable"),
                target);
        } else if (state.step.completion != CompletionState::Completed) {
            addIssue(result, view,
                QStringLiteral("workflow.action.not_completed"),
                QStringLiteral("required workflow step is not completed"),
                target);
        } else if (state.step.validation != ValidationState::Valid) {
            addIssue(result, view,
                QStringLiteral("workflow.action.validation_required"),
                QStringLiteral("required workflow validation is not current and valid"),
                target);
        } else if (!state.step.blockers.isEmpty()) {
            const DependencyBlockerView &blocker = state.step.blockers.first();
            addIssue(result, view,
                QStringLiteral("workflow.action.dependency_blocked"),
                blocker.message.isEmpty()
                    ? QStringLiteral("required workflow dependency is blocked")
                    : blocker.message,
                blocker.targetPageId.isEmpty() ? target
                                               : blocker.targetPageId);
        } else if (!state.inputVersionMatches) {
            addIssue(result, view,
                QStringLiteral("workflow.action.input_changed"),
                QStringLiteral("workflow input changed after validation"),
                target);
        }
    }

    result.action.enabled = result.issues.isEmpty();
    if (!result.action.enabled) {
        result.action.reason = result.issues.first().message;
        result.action.targetPageId = result.issues.first().targetPageId;
    }
    return result;
}

} // namespace workflow
} // namespace aerosim
