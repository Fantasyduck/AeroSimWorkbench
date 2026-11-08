#include "aerosim/workflow/LegacyWorkflowAdapter.h"

namespace aerosim {
namespace workflow {

WorkflowView toLegacyWorkflowView(const WorkflowViewV2 &source)
{
    WorkflowView result;
    result.scope = source.scope;
    result.viewRevision = source.viewRevision;
    result.actions = source.actions;
    result.issues = source.issues;
    result.nextSuggestion = source.nextSuggestion;
    for (const WorkflowStepViewV2 &step : source.steps) {
        WorkflowStepView legacy;
        legacy.stepId = step.stepId;
        legacy.scopeId = step.scope.caseId;
        legacy.summary = step.summary;
        legacy.targetPageId = step.targetPageId;
        if (!step.blockers.isEmpty()) {
            legacy.state = StepState::Blocked;
        } else if (!step.applicable) {
            legacy.state = StepState::Pending;
        } else if (step.validation == ValidationState::Stale
                   || step.validation == ValidationState::Invalid) {
            legacy.state = StepState::NeedsValidation;
        } else if (step.completion == CompletionState::Completed
                   && step.validation == ValidationState::Valid) {
            legacy.state = StepState::Complete;
        } else {
            legacy.state = StepState::Pending;
        }
        result.steps.append(legacy);
    }
    return result;
}

} // namespace workflow
} // namespace aerosim
