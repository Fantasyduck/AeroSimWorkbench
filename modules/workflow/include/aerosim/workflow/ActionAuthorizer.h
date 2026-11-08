#pragma once

#include "aerosim/workflow/DependencyEvaluator.h"

namespace aerosim {
namespace workflow {

struct ActionAuthorization {
    WorkflowActionView action;
    QVector<ValidationIssue> issues;

    bool allowed() const { return action.enabled; }
};

ActionAuthorization authorizeAction(
    const WorkflowActionRule &rule,
    const WorkflowViewV2 &view,
    const WorkflowStepEvaluationStateMap &states,
    quint64 *operationCount = nullptr);

} // namespace workflow
} // namespace aerosim
