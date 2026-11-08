#include "aerosim/workflow/WorkflowDefinition.h"

#include <QSet>

#include <utility>

namespace aerosim {
namespace workflow {
namespace {

AccessResult<WorkflowDefinition> invalidDefinition(
    const QString &code, const QString &message)
{
    return AccessResult<WorkflowDefinition>::failure(
        AccessStatus::InvalidArgument, code, message);
}

} // namespace

AccessResult<WorkflowDefinition>
validateWorkflowDefinition(WorkflowDefinition definition)
{
    if (definition.definitionVersion.isEmpty()) {
        return invalidDefinition(
            QStringLiteral("workflow.definition.empty_version"),
            QStringLiteral("Workflow definitionVersion must not be empty"));
    }

    QSet<QString> stepIds;
    for (const WorkflowStepDefinition &step : definition.steps) {
        if (step.stepId.isEmpty()) {
            return invalidDefinition(
                QStringLiteral("workflow.definition.empty_step_id"),
                QStringLiteral("Workflow stepId must not be empty"));
        }
        if (stepIds.contains(step.stepId)) {
            return invalidDefinition(
                QStringLiteral("workflow.definition.duplicate_step_id"),
                QStringLiteral("Workflow stepId must be unique"));
        }
        stepIds.insert(step.stepId);

        QSet<QString> requirementIds;
        QSet<QString> blockerIdentities;
        for (const DependencyRequirement &requirement : step.dependencies) {
            if (requirement.requirementId.isEmpty()) {
                return invalidDefinition(
                    QStringLiteral("workflow.definition.empty_requirement_id"),
                    QStringLiteral("Dependency requirementId must not be empty"));
            }
            if (requirementIds.contains(requirement.requirementId)) {
                return invalidDefinition(
                    QStringLiteral("workflow.definition.duplicate_requirement_id"),
                    QStringLiteral(
                        "Dependency requirementId must be unique within a step"));
            }
            requirementIds.insert(requirement.requirementId);

            const QString blockerIdentity = requirement.dependencyStepId
                + QChar(0x1f) + requirement.code;
            if (blockerIdentities.contains(blockerIdentity)) {
                return invalidDefinition(
                    QStringLiteral(
                        "workflow.definition.duplicate_blocker_identity"),
                    QStringLiteral(
                        "Dependency blocker identity must be unique within a step"));
            }
            blockerIdentities.insert(blockerIdentity);
        }
    }

    QSet<QString> actionIds;
    for (const WorkflowActionRule &action : definition.actions) {
        if (action.actionId.isEmpty()) {
            return invalidDefinition(
                QStringLiteral("workflow.definition.empty_action_id"),
                QStringLiteral("Workflow actionId must not be empty"));
        }
        if (actionIds.contains(action.actionId)) {
            return invalidDefinition(
                QStringLiteral("workflow.definition.duplicate_action_id"),
                QStringLiteral("Workflow actionId must be unique"));
        }
        actionIds.insert(action.actionId);
    }

    return AccessResult<WorkflowDefinition>::success(std::move(definition));
}

} // namespace workflow
} // namespace aerosim
