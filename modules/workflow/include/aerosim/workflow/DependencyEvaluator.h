#pragma once

#include "aerosim/workflow/WorkflowDefinition.h"
#include "aerosim/contracts/WorkflowModels.h"

#include <QHash>

namespace aerosim {
namespace workflow {

struct WorkflowStepEvaluationState {
    WorkflowStepViewV2 step;
    bool inputVersionMatches = false;
};

using WorkflowStepEvaluationStateMap =
    QHash<QString, WorkflowStepEvaluationState>;

QVector<DependencyBlockerView> evaluateDependencies(
    const WorkflowStepDefinition &definition,
    const WorkflowStepEvaluationStateMap &states,
    quint64 *operationCount = nullptr);

} // namespace workflow
} // namespace aerosim
