#include "WorkflowTestSupport.h"

#include "aerosim/workflow/WorkflowStateDeriver.h"

#include <QCoreApplication>

namespace {

struct TraceEntry {
    const char *requirement;
    const char *testName;
};

const TraceEntry kTrace[] = {
    {"FR-001", "contracts_workflow_v2"},
    {"FR-002", "workflow_state_derivation"},
    {"FR-003", "workflow_state_derivation"},
    {"FR-004", "workflow_state_derivation"},
    {"FR-005", "workflow_state_derivation"},
    {"FR-006", "workflow_input_stamp"},
    {"FR-007", "workflow_input_stamp"},
    {"FR-008", "workflow_input_stamp"},
    {"FR-009", "workflow_input_stamp"},
    {"FR-010", "workflow_input_stamp"},
    {"FR-011", "workflow_async_validation"},
    {"FR-012", "workflow_action_authorization"},
    {"FR-013", "workflow_dependency_blockers"},
    {"FR-014", "workflow_dependency_blockers"},
    {"FR-015", "workflow_action_authorization"},
    {"FR-016", "workflow_legacy_adapter"},
    {"FR-017", "workflow_legacy_adapter"},
    {"FR-018", "mock_workflow_v2"},
    {"FR-019", "workflow_snapshot_roundtrip"},
    {"FR-020", "workflow_snapshot_roundtrip"},
    {"FR-021", "workflow_snapshot_roundtrip"},
    {"FR-022", "workflow_action_authorization"},
    {"FR-023", "workflow_acceptance"}
};

aerosim::ScopeRef scope()
{
    aerosim::ScopeRef value;
    value.projectId = QStringLiteral("performance-project");
    value.caseId = QStringLiteral("performance-case");
    value.objectId = QStringLiteral("performance-state");
    return value;
}

aerosim::workflow::ValidationInputStamp stamp(int index)
{
    aerosim::workflow::ValidationInputStamp value;
    value.scope = scope();
    value.definitionVersion = QStringLiteral("workflow-v2");
    value.parameter.snapshotId = QStringLiteral("parameters-%1").arg(index);
    value.parameter.scope = value.scope;
    value.parameter.revision = static_cast<quint64>(index + 1);
    aerosim::workflow::ResourceInputStamp resource;
    resource.projectId = value.scope.projectId;
    resource.resourceId = QStringLiteral("mesh-%1").arg(index);
    resource.revision = static_cast<quint64>(index + 1);
    value.resources.append(resource);
    return value;
}

aerosim::workflow::WorkflowDerivationContext fixture(int size)
{
    aerosim::workflow::WorkflowDerivationContext context;
    context.scope = scope();
    context.definition.definitionVersion = QStringLiteral("workflow-v2");
    context.facts.scope = context.scope;
    context.facts.definitionVersion = context.definition.definitionVersion;
    for (int index = 0; index < size; ++index) {
        const QString stepId = QStringLiteral("step-%1").arg(index);
        aerosim::workflow::WorkflowStepDefinition step;
        step.stepId = stepId;
        step.applicabilityRuleId = QStringLiteral("always");
        if (index > 0) {
            aerosim::workflow::DependencyRequirement dependency;
            dependency.dependencyStepId =
                QStringLiteral("step-%1").arg(index - 1);
            dependency.conditionKind = aerosim::workflow::
                DependencyConditionKind::CurrentValidationValid;
            dependency.code = QStringLiteral("upstream.invalid.%1").arg(index);
            step.dependencies.append(dependency);
        }
        context.definition.steps.append(step);

        aerosim::workflow::WorkflowStepFacts facts;
        facts.stepId = stepId;
        facts.committedCompletion = aerosim::CompletionState::Completed;
        facts.hasValidationFact = true;
        facts.validationFact.outcome =
            aerosim::workflow::ValidationOutcome::Passed;
        facts.validationFact.inputStamp = stamp(index);
        facts.factRevision = static_cast<quint64>(index + 1);
        context.facts.steps.append(facts);
        context.facts.factRevision = facts.factRevision;

        aerosim::workflow::WorkflowStepCurrentInput input;
        input.stepId = stepId;
        input.inputStamp = stamp(index);
        context.currentInputs.append(input);

        aerosim::workflow::WorkflowActionRule action;
        action.actionId = QStringLiteral("run-%1").arg(index);
        action.requiresCurrentValidation = true;
        action.requiredStepIds.append(stepId);
        context.definition.actions.append(action);
    }
    return context;
}

quint64 deriveCount(int size, aerosim_workflow_test::TestContext &test)
{
    quint64 operations = 0;
    const aerosim::WorkflowViewV2 view =
        aerosim::workflow::deriveWorkflowView(fixture(size), &operations);
    AEROSIM_WORKFLOW_CHECK(test, view.steps.size() == size);
    AEROSIM_WORKFLOW_CHECK(test, view.actions.size() == size);
    return operations;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application);
    aerosim_workflow_test::TestContext context;

    AEROSIM_WORKFLOW_CHECK(context,
        sizeof(kTrace) / sizeof(kTrace[0]) == 23);
    for (const TraceEntry &entry : kTrace) {
        AEROSIM_WORKFLOW_CHECK(context,
            entry.requirement[0] != '\0' && entry.testName[0] != '\0');
    }

    // Deterministic operation counters are the assertion. Wall-clock timing
    // is deliberately not used as a pass/fail criterion.
    const quint64 one = deriveCount(16, context);
    const quint64 two = deriveCount(32, context);
    const quint64 four = deriveCount(64, context);
    AEROSIM_WORKFLOW_CHECK(context, one > 0);
    AEROSIM_WORKFLOW_CHECK(context, two > one && two <= one * 3);
    AEROSIM_WORKFLOW_CHECK(context, four > two && four <= two * 3);

    return context.result();
}
