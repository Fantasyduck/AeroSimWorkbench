#include "aerosim/workflow/DependencyEvaluator.h"

namespace aerosim {
namespace workflow {

QVector<DependencyBlockerView> evaluateDependencies(
    const WorkflowStepDefinition &definition,
    const WorkflowStepEvaluationStateMap &states,
    quint64 *operationCount)
{
    QVector<DependencyBlockerView> blockers;
    for (const DependencyRequirement &requirement : definition.dependencies) {
        if (!requirement.applicable)
            continue;
        if (operationCount)
            ++(*operationCount);

        bool satisfied = false;
        const auto found = states.constFind(requirement.dependencyStepId);
        if (found != states.constEnd()) {
            const WorkflowStepEvaluationState &upstream = found.value();
            switch (requirement.conditionKind) {
            case DependencyConditionKind::CompletionSatisfied:
                satisfied = upstream.step.applicable
                    && upstream.step.completion == CompletionState::Completed;
                break;
            case DependencyConditionKind::CurrentValidationValid:
                satisfied = upstream.step.applicable
                    && upstream.step.validation == ValidationState::Valid;
                break;
            case DependencyConditionKind::InputVersionMatches:
                satisfied = upstream.step.applicable
                    && upstream.inputVersionMatches;
                break;
            }
        }
        if (satisfied)
            continue;

        DependencyBlockerView blocker;
        blocker.dependencyStepId = requirement.dependencyStepId;
        blocker.code = requirement.code;
        blocker.message = requirement.message;
        blocker.targetPageId = requirement.targetPageId;
        blockers.append(blocker);
    }
    return blockers;
}

} // namespace workflow
} // namespace aerosim
