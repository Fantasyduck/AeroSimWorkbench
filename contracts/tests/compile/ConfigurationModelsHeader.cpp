#include "aerosim/contracts/ConfigurationModels.h"

#include <type_traits>

static_assert(std::is_copy_constructible<aerosim::ParameterSnapshot>::value,
    "ParameterSnapshot must remain a copyable value type");
static_assert(std::is_copy_constructible<aerosim::BatchPlan>::value,
    "BatchPlan must remain a copyable value type");
static_assert(std::is_copy_constructible<aerosim::StateConfig>::value,
    "StateConfig must remain a copyable value type");
static_assert(std::is_copy_constructible<aerosim::BatchPlanExecutionSnapshot>::value,
    "BatchPlanExecutionSnapshot must remain a copyable value type");

int compileConfigurationModelsHeader()
{
    aerosim::StateConfig state;
    aerosim::BatchPlan plan;
    plan.items.append(aerosim::BatchPlanItem());
    return state.name.isEmpty() && plan.items.size() == 1 ? 0 : 1;
}
