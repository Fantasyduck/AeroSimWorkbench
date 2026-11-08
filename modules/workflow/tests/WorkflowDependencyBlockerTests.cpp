#include "WorkflowTestSupport.h"

#include "aerosim/workflow/DependencyEvaluator.h"
#include "aerosim/workflow/WorkflowStateDeriver.h"

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

aerosim::workflow::DependencyRequirement requirement(
    const QString &id, aerosim::workflow::DependencyConditionKind kind,
    const QString &code)
{
    aerosim::workflow::DependencyRequirement result;
    result.requirementId = id;
    result.dependentStepId = QStringLiteral("downstream");
    result.dependencyStepId = QStringLiteral("upstream");
    result.conditionKind = kind;
    result.code = code;
    result.message = code + QStringLiteral(" is required");
    result.targetPageId = QStringLiteral("upstream-page");
    return result;
}

const aerosim::WorkflowStepViewV2 *findStep(
    const aerosim::WorkflowViewV2 &view, const QString &stepId)
{
    for (const aerosim::WorkflowStepViewV2 &step : view.steps) {
        if (step.stepId == stepId)
            return &step;
    }
    return nullptr;
}

} // namespace

int main()
{
    aerosim_workflow_test::TestContext context;

    aerosim::workflow::WorkflowStepDefinition downstream;
    downstream.stepId = QStringLiteral("downstream");
    downstream.dependencies.append(requirement(
        QStringLiteral("completion"),
        aerosim::workflow::DependencyConditionKind::CompletionSatisfied,
        QStringLiteral("upstream.completion")));
    downstream.dependencies.append(requirement(
        QStringLiteral("validation"),
        aerosim::workflow::DependencyConditionKind::CurrentValidationValid,
        QStringLiteral("upstream.validation")));
    downstream.dependencies.append(requirement(
        QStringLiteral("version"),
        aerosim::workflow::DependencyConditionKind::InputVersionMatches,
        QStringLiteral("upstream.version")));
    aerosim::workflow::DependencyRequirement ignored = requirement(
        QStringLiteral("ignored"),
        aerosim::workflow::DependencyConditionKind::CompletionSatisfied,
        QStringLiteral("ignored"));
    ignored.dependencyStepId = QStringLiteral("unrelated");
    ignored.applicable = false;
    downstream.dependencies.append(ignored);

    aerosim::workflow::WorkflowStepEvaluationState upstream;
    upstream.step.stepId = QStringLiteral("upstream");
    upstream.step.applicable = true;
    aerosim::workflow::WorkflowStepEvaluationStateMap states;
    states.insert(upstream.step.stepId, upstream);

    QVector<aerosim::DependencyBlockerView> blockers =
        aerosim::workflow::evaluateDependencies(downstream, states);
    AEROSIM_WORKFLOW_CHECK(context, blockers.size() == 3);
    AEROSIM_WORKFLOW_CHECK(context,
        blockers.at(0).dependencyStepId == QStringLiteral("upstream"));
    AEROSIM_WORKFLOW_CHECK(context,
        blockers.at(0).targetPageId == QStringLiteral("upstream-page"));

    states[QStringLiteral("upstream")].step.completion =
        aerosim::CompletionState::Completed;
    blockers = aerosim::workflow::evaluateDependencies(downstream, states);
    AEROSIM_WORKFLOW_CHECK(context, blockers.size() == 2);
    states[QStringLiteral("upstream")].step.validation =
        aerosim::ValidationState::Valid;
    blockers = aerosim::workflow::evaluateDependencies(downstream, states);
    AEROSIM_WORKFLOW_CHECK(context, blockers.size() == 1);
    AEROSIM_WORKFLOW_CHECK(context,
        blockers.first().code == QStringLiteral("upstream.version"));
    states[QStringLiteral("upstream")].inputVersionMatches = true;
    AEROSIM_WORKFLOW_CHECK(context,
        aerosim::workflow::evaluateDependencies(downstream, states).isEmpty());

    // A recovered upstream removes its blocker, while the downstream keeps
    // its independent completion fact and stale validation state.
    aerosim::workflow::WorkflowDerivationContext derivation;
    derivation.scope = scope();
    derivation.definition.definitionVersion = QStringLiteral("v2");
    aerosim::workflow::WorkflowStepDefinition upstreamDefinition;
    upstreamDefinition.stepId = QStringLiteral("upstream");
    upstreamDefinition.applicabilityRuleId = QStringLiteral("always");
    derivation.definition.steps.append(upstreamDefinition);
    aerosim::workflow::WorkflowStepDefinition dependentDefinition;
    dependentDefinition.stepId = QStringLiteral("downstream");
    dependentDefinition.applicabilityRuleId = QStringLiteral("always");
    dependentDefinition.dependencies.append(requirement(
        QStringLiteral("upstream-valid"),
        aerosim::workflow::DependencyConditionKind::CurrentValidationValid,
        QStringLiteral("upstream.validation")));
    derivation.definition.steps.append(dependentDefinition);
    derivation.facts.scope = derivation.scope;
    derivation.facts.definitionVersion = QStringLiteral("v2");
    aerosim::workflow::WorkflowStepFacts upstreamFacts;
    upstreamFacts.stepId = QStringLiteral("upstream");
    upstreamFacts.committedCompletion = aerosim::CompletionState::Completed;
    upstreamFacts.hasValidationFact = true;
    upstreamFacts.validationFact.outcome =
        aerosim::workflow::ValidationOutcome::Passed;
    upstreamFacts.validationFact.inputStamp = stamp(2);
    derivation.facts.steps.append(upstreamFacts);
    aerosim::workflow::WorkflowStepFacts downstreamFacts = upstreamFacts;
    downstreamFacts.stepId = QStringLiteral("downstream");
    downstreamFacts.validationFact.inputStamp = stamp(1);
    derivation.facts.steps.append(downstreamFacts);
    aerosim::workflow::WorkflowStepCurrentInput upstreamInput;
    upstreamInput.stepId = QStringLiteral("upstream");
    upstreamInput.inputStamp = stamp(2);
    derivation.currentInputs.append(upstreamInput);
    aerosim::workflow::WorkflowStepCurrentInput downstreamInput;
    downstreamInput.stepId = QStringLiteral("downstream");
    downstreamInput.inputStamp = stamp(2);
    derivation.currentInputs.append(downstreamInput);

    const aerosim::WorkflowViewV2 view =
        aerosim::workflow::deriveWorkflowView(derivation);
    const aerosim::WorkflowStepViewV2 *dependent =
        findStep(view, QStringLiteral("downstream"));
    AEROSIM_WORKFLOW_CHECK(context, dependent != nullptr);
    AEROSIM_WORKFLOW_CHECK(context, dependent->blockers.isEmpty());
    AEROSIM_WORKFLOW_CHECK(context,
        dependent->completion == aerosim::CompletionState::Completed);
    AEROSIM_WORKFLOW_CHECK(context,
        dependent->validation == aerosim::ValidationState::Stale);

    return context.result();
}
