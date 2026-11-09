#include "aerosim/contracts/ExecutionModels.h"

#include <type_traits>

static_assert(std::is_copy_constructible<aerosim::TaskInputManifest>::value,
    "TaskInputManifest must remain a copyable value type");
static_assert(std::is_copy_constructible<aerosim::BatchRun>::value,
    "BatchRun must remain a copyable value type");

int compileExecutionModelsHeader()
{
    aerosim::TaskAttempt attempt;
    attempt.sequence = 1;
    return attempt.isValid() ? 1 : 0;
}
