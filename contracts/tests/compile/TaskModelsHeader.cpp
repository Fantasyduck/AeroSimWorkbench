#include "aerosim/contracts/TaskModels.h"

#include <type_traits>

static_assert(std::is_copy_constructible<aerosim::TaskSnapshot>::value,
    "TaskSnapshot must remain a copyable value type");
static_assert(std::is_copy_constructible<aerosim::TaskAttempt>::value,
    "TaskAttempt must remain a copyable value type");

int compileTaskModelsHeader()
{
    aerosim::TaskSnapshot snapshot;
    snapshot.attempts.append(aerosim::TaskAttempt());
    return snapshot.attempts.size() == 1 ? 0 : 1;
}

