#pragma once

#include "aerosim/contracts/AccessResult.h"

#include <QString>
#include <QVector>

namespace aerosim {
namespace workflow {

enum class DependencyConditionKind {
    CompletionSatisfied,
    CurrentValidationValid,
    InputVersionMatches
};

struct DependencyRequirement {
    QString requirementId;
    QString dependentStepId;
    QString dependencyStepId;
    DependencyConditionKind conditionKind =
        DependencyConditionKind::CompletionSatisfied;
    QString code;
    QString message;
    QString targetPageId;
    bool applicable = true;
};

struct WorkflowStepDefinition {
    QString stepId;
    QString summary;
    QString targetPageId;
    QString applicabilityRuleId;
    bool applicable = true;
    QVector<DependencyRequirement> dependencies;
};

struct WorkflowActionRule {
    QString actionId;
    QString targetPageId;
    QVector<QString> requiredStepIds;
    bool requiresCurrentValidation = false;
};

struct WorkflowDefinition {
    QString definitionVersion;
    QVector<WorkflowStepDefinition> steps;
    QVector<WorkflowActionRule> actions;
};

AccessResult<WorkflowDefinition>
validateWorkflowDefinition(WorkflowDefinition definition);

} // namespace workflow
} // namespace aerosim
