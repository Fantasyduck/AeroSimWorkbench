#include "aerosim/workflow/ValidationCoordinator.h"

#include "aerosim/workflow/ValidationStampMatcher.h"

#include <limits>

namespace aerosim {
namespace workflow {
namespace {

bool sameScope(const ScopeRef &left, const ScopeRef &right)
{
    return left.projectId == right.projectId
        && left.caseId == right.caseId
        && left.objectId == right.objectId;
}

bool sameKey(const PendingValidation &pending, const ScopeRef &scope,
             const QString &stepId)
{
    return sameScope(pending.scope, scope) && pending.stepId == stepId;
}

} // namespace

AccessResult<PendingValidation> ValidationCoordinator::begin(
    const QString &requestId, const ScopeRef &scope, const QString &stepId,
    const ValidationInputStamp &inputStamp)
{
    if (shutdown_) {
        return AccessResult<PendingValidation>::failure(
            AccessStatus::Unavailable,
            QStringLiteral("workflow.validation.shutdown"),
            QStringLiteral("validation coordinator is shut down"));
    }
    const AccessResult<ValidationInputStamp> normalized =
        normalizeValidationInputStamp(inputStamp);
    if (requestId.isEmpty() || stepId.isEmpty() || !normalized.ok()
        || !sameScope(scope, inputStamp.scope)) {
        return AccessResult<PendingValidation>::failure(
            AccessStatus::InvalidArgument,
            QStringLiteral("workflow.validation.invalid_request"),
            QStringLiteral("validation request is incomplete"));
    }
    if (nextGeneration_ == std::numeric_limits<quint64>::max()) {
        return AccessResult<PendingValidation>::failure(
            AccessStatus::Unavailable,
            QStringLiteral("workflow.validation.generation_exhausted"),
            QStringLiteral("validation generation is exhausted"));
    }

    PendingValidation pending;
    pending.requestId = requestId;
    pending.scope = scope;
    pending.stepId = stepId;
    pending.generation = nextGeneration_++;
    pending.inputStamp = normalized.value();
    cancel(scope, stepId);
    pending_.append(pending);
    return AccessResult<PendingValidation>::success(pending);
}

bool ValidationCoordinator::accept(
    const ValidationCompletion &completion,
    const ValidationInputStamp &currentInputStamp)
{
    if (shutdown_)
        return false;
    for (int index = 0; index < pending_.size(); ++index) {
        const PendingValidation pending = pending_.at(index);
        if (!sameKey(pending, completion.scope, completion.stepId))
            continue;
        if (pending.requestId != completion.requestId
            || pending.generation != completion.generation) {
            return false;
        }
        const AccessResult<bool> requestStampMatches =
            validationInputStampsMatch(pending.inputStamp,
                                       completion.inputStamp);
        const AccessResult<bool> currentStampMatches =
            validationInputStampsMatch(completion.inputStamp,
                                       currentInputStamp);
        pending_.removeAt(index);
        return requestStampMatches.ok() && requestStampMatches.value()
            && currentStampMatches.ok() && currentStampMatches.value();
    }
    return false;
}

void ValidationCoordinator::cancel(const ScopeRef &scope,
                                   const QString &stepId)
{
    for (int index = pending_.size() - 1; index >= 0; --index) {
        if (sameKey(pending_.at(index), scope, stepId))
            pending_.removeAt(index);
    }
}

void ValidationCoordinator::destroyScope(const ScopeRef &scope)
{
    for (int index = pending_.size() - 1; index >= 0; --index) {
        if (sameScope(pending_.at(index).scope, scope))
            pending_.removeAt(index);
    }
}

void ValidationCoordinator::shutdown()
{
    shutdown_ = true;
    pending_.clear();
}

} // namespace workflow
} // namespace aerosim
