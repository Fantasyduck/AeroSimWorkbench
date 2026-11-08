#include "WorkflowTestSupport.h"

#include "aerosim/workflow/WorkflowDefinition.h"
#include "aerosim/workflow/WorkflowFacts.h"

namespace {

aerosim::workflow::DependencyRequirement requirement(
    const QString &id, const QString &dependencyStepId, const QString &code,
    aerosim::workflow::DependencyConditionKind kind)
{
    aerosim::workflow::DependencyRequirement value;
    value.requirementId = id;
    value.dependentStepId = QStringLiteral("dependent");
    value.dependencyStepId = dependencyStepId;
    value.conditionKind = kind;
    value.code = code;
    value.message = QStringLiteral("Resolve dependency");
    value.targetPageId = QStringLiteral("configuration");
    return value;
}

aerosim::workflow::WorkflowDefinition validDefinition()
{
    using namespace aerosim::workflow;
    WorkflowDefinition definition;
    definition.definitionVersion = QStringLiteral("workflow-v2-test");

    WorkflowStepDefinition upstream;
    upstream.stepId = QStringLiteral("upstream");
    upstream.applicabilityRuleId = QStringLiteral("always");
    definition.steps.append(upstream);

    WorkflowStepDefinition dependent;
    dependent.stepId = QStringLiteral("dependent");
    dependent.applicabilityRuleId = QStringLiteral("always");
    dependent.dependencies.append(requirement(
        QStringLiteral("completion"), QStringLiteral("upstream"),
        QStringLiteral("dependency.completion"),
        DependencyConditionKind::CompletionSatisfied));
    dependent.dependencies.append(requirement(
        QStringLiteral("validation"), QStringLiteral("upstream"),
        QStringLiteral("dependency.validation"),
        DependencyConditionKind::CurrentValidationValid));
    dependent.dependencies.append(requirement(
        QStringLiteral("version"), QStringLiteral("upstream"),
        QStringLiteral("dependency.version"),
        DependencyConditionKind::InputVersionMatches));
    definition.steps.append(dependent);

    WorkflowActionRule run;
    run.actionId = QStringLiteral("run");
    run.requiredStepIds.append(QStringLiteral("dependent"));
    run.requiresCurrentValidation = true;
    definition.actions.append(run);
    return definition;
}

void checkFailure(aerosim_workflow_test::TestContext &context,
                  const aerosim::workflow::WorkflowDefinition &definition,
                  const QString &expectedCode)
{
    const aerosim::AccessResult<aerosim::workflow::WorkflowDefinition> result =
        aerosim::workflow::validateWorkflowDefinition(definition);
    AEROSIM_WORKFLOW_CHECK(context, !result.ok());
    AEROSIM_WORKFLOW_CHECK(
        context, result.status() == aerosim::AccessStatus::InvalidArgument);
    AEROSIM_WORKFLOW_CHECK(context, result.errorCode() == expectedCode);
}

} // namespace

int main()
{
    using namespace aerosim::workflow;
    aerosim_workflow_test::TestContext context;

    const WorkflowDefinition baseline = validDefinition();
    const aerosim::AccessResult<WorkflowDefinition> valid =
        validateWorkflowDefinition(baseline);
    AEROSIM_WORKFLOW_CHECK(context, valid.ok());
    AEROSIM_WORKFLOW_CHECK(context,
        valid.value().definitionVersion == baseline.definitionVersion);
    AEROSIM_WORKFLOW_CHECK(context,
        valid.value().steps.at(1).dependencies.size() == 3);
    AEROSIM_WORKFLOW_CHECK(context,
        baseline.steps.at(1).dependencies.at(0).conditionKind
            != baseline.steps.at(1).dependencies.at(1).conditionKind);
    AEROSIM_WORKFLOW_CHECK(context,
        baseline.steps.at(1).dependencies.at(1).conditionKind
            != baseline.steps.at(1).dependencies.at(2).conditionKind);

    WorkflowDefinition invalid = baseline;
    invalid.definitionVersion.clear();
    checkFailure(context, invalid,
        QStringLiteral("workflow.definition.empty_version"));

    invalid = baseline;
    invalid.steps.append(invalid.steps.first());
    checkFailure(context, invalid,
        QStringLiteral("workflow.definition.duplicate_step_id"));

    invalid = baseline;
    invalid.actions.append(invalid.actions.first());
    checkFailure(context, invalid,
        QStringLiteral("workflow.definition.duplicate_action_id"));

    invalid = baseline;
    invalid.steps[1].dependencies[1].requirementId =
        invalid.steps[1].dependencies[0].requirementId;
    checkFailure(context, invalid,
        QStringLiteral("workflow.definition.duplicate_requirement_id"));

    invalid = baseline;
    invalid.steps[1].dependencies[1].dependencyStepId =
        invalid.steps[1].dependencies[0].dependencyStepId;
    invalid.steps[1].dependencies[1].code =
        invalid.steps[1].dependencies[0].code;
    checkFailure(context, invalid,
        QStringLiteral("workflow.definition.duplicate_blocker_identity"));

    WorkflowDefinition sameIdentityDifferentStep = baseline;
    WorkflowStepDefinition other;
    other.stepId = QStringLiteral("other-dependent");
    other.dependencies.append(requirement(
        QStringLiteral("completion"), QStringLiteral("upstream"),
        QStringLiteral("dependency.completion"),
        DependencyConditionKind::CompletionSatisfied));
    sameIdentityDifferentStep.steps.append(other);
    AEROSIM_WORKFLOW_CHECK(context,
        validateWorkflowDefinition(sameIdentityDifferentStep).ok());

    WorkflowDefinition sameDependencyDifferentCode = baseline;
    sameDependencyDifferentCode.steps[1].dependencies[1].dependencyStepId =
        sameDependencyDifferentCode.steps[1].dependencies[0].dependencyStepId;
    AEROSIM_WORKFLOW_CHECK(context,
        validateWorkflowDefinition(sameDependencyDifferentCode).ok());

    WorkflowScopeFacts facts;
    facts.definitionVersion = baseline.definitionVersion;
    AEROSIM_WORKFLOW_CHECK(context, facts.steps.isEmpty());

    return context.result();
}
