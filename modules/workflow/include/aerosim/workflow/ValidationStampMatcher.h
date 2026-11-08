#pragma once

#include "aerosim/contracts/AccessResult.h"
#include "aerosim/workflow/WorkflowFacts.h"

namespace aerosim {
namespace workflow {

bool validationInputStampIsValid(const ValidationInputStamp &stamp);

AccessResult<ValidationInputStamp>
normalizeValidationInputStamp(const ValidationInputStamp &stamp);

AccessResult<bool> validationInputStampsMatch(
    const ValidationInputStamp &validated,
    const ValidationInputStamp &current,
    quint64 *operationCount = nullptr);

} // namespace workflow
} // namespace aerosim
