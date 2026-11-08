#include "aerosim/workflow/WorkflowStateDeriver.h"

#include "aerosim/workflow/ValidationStampMatcher.h"

#include <QHash>
#include <QSet>

namespace aerosim {
namespace workflow {
namespace {

struct ValidationDerivation {
    ValidationState state = ValidationState::NotValidated;
    bool inputVersionMatches = false;
};

ValidationDerivation deriveValidation(const WorkflowStepFacts *facts,
                                      const ValidationInputStamp *currentInput,
                                      const QString &currentDefinitionVersion,
                                      quint64 *operationCount)
{
    ValidationDerivation result;
    if (!facts || !facts->hasValidationFact
        || !validationInputStampIsValid(facts->validationFact.inputStamp)) {
        return result;
    }
    if (!currentInput) {
        result.state = ValidationState::Stale;
        return result;
    }
    if (facts->validationFact.inputStamp.definitionVersion
            != currentDefinitionVersion
        || currentInput->definitionVersion != currentDefinitionVersion) {
        result.state = ValidationState::Stale;
        return result;
    }
    const AccessResult<bool> match = validationInputStampsMatch(
        facts->validationFact.inputStamp, *currentInput, operationCount);
    if (!match.ok() || !match.value()) {
        result.state = ValidationState::Stale;
        return result;
    }
    result.inputVersionMatches = true;
    result.state = facts->validationFact.outcome == ValidationOutcome::Passed
        ? ValidationState::Valid
        : ValidationState::Invalid;
    return result;
}

struct DerivationResult {
    WorkflowViewV2 view;
    WorkflowStepEvaluationStateMap states;
};

DerivationResult deriveDetailed(const WorkflowDerivationContext &context,
                                quint64 *operationCount)
{
    DerivationResult result;
    result.view.scope = context.scope;
    result.view.definitionVersion = context.definition.definitionVersion;
    result.view.viewRevision = context.viewRevision;

    QHash<QString, const WorkflowStepFacts *> factsByStep;
    for (const WorkflowStepFacts &facts : context.facts.steps) {
        factsByStep.insert(facts.stepId, &facts);
        if (operationCount)
            ++(*operationCount);
    }
    QHash<QString, const ValidationInputStamp *> inputsByStep;
    for (const WorkflowStepCurrentInput &input : context.currentInputs) {
        inputsByStep.insert(input.stepId, &input.inputStamp);
        if (operationCount)
            ++(*operationCount);
    }
    QSet<QString> draftSteps;
    for (const EditingDraftOverlay &draft : context.drafts) {
        draftSteps.insert(draft.stepId);
        if (operationCount)
            ++(*operationCount);
    }

    for (const WorkflowStepDefinition &definition : context.definition.steps) {
        if (operationCount)
            ++(*operationCount);
        const WorkflowStepFacts *facts = factsByStep.value(
            definition.stepId, nullptr);
        WorkflowStepViewV2 step;
        step.stepId = definition.stepId;
        step.scope = context.scope;
        step.applicable = definition.applicable;
        step.applicabilityRuleId = definition.applicabilityRuleId;
        step.completion = facts ? facts->committedCompletion
                                : CompletionState::NotStarted;
        if (draftSteps.contains(definition.stepId))
            step.completion = CompletionState::InProgress;
        const ValidationDerivation validation = deriveValidation(
            facts, inputsByStep.value(definition.stepId, nullptr),
            context.definition.definitionVersion, operationCount);
        step.validation = validation.state;
        step.summary = definition.summary;
        step.targetPageId = definition.targetPageId;
        step.factRevision = facts ? facts->factRevision : 0;
        result.view.steps.append(step);

        WorkflowStepEvaluationState state;
        state.step = step;
        state.inputVersionMatches = validation.inputVersionMatches;
        result.states.insert(step.stepId, state);

        if (facts && facts->hasValidationFact) {
            for (const ValidationIssue &issue : facts->validationFact.issues)
                result.view.issues.append(issue);
        }
    }

    for (int index = 0; index < context.definition.steps.size(); ++index) {
        WorkflowStepViewV2 &step = result.view.steps[index];
        step.blockers = evaluateDependencies(context.definition.steps.at(index),
                                             result.states, operationCount);
        auto found = result.states.find(step.stepId);
        if (found != result.states.end())
            found.value().step = step;
    }

    for (const WorkflowActionRule &rule : context.definition.actions) {
        result.view.actions.append(
            authorizeAction(rule, result.view, result.states,
                            operationCount).action);
    }
    return result;
}

} // namespace

WorkflowViewV2 deriveWorkflowView(const WorkflowDerivationContext &context,
                                  quint64 *operationCount)
{
    return deriveDetailed(context, operationCount).view;
}

ActionAuthorization authorizeWorkflowAction(
    const WorkflowDerivationContext &context, const QString &actionId)
{
    const DerivationResult derived = deriveDetailed(context, nullptr);
    for (const WorkflowActionRule &rule : context.definition.actions) {
        if (rule.actionId == actionId)
            return authorizeAction(rule, derived.view, derived.states);
    }
    ActionAuthorization missing;
    missing.action.actionId = actionId;
    ValidationIssue issue;
    issue.code = QStringLiteral("workflow.action.not_configured");
    issue.message = QStringLiteral("workflow action is not configured");
    issue.scopeId = context.scope.caseId;
    issue.objectId = context.scope.objectId;
    missing.issues.append(issue);
    missing.action.reason = issue.message;
    return missing;
}

} // namespace workflow
} // namespace aerosim
