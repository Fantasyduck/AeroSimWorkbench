#include "aerosim/contracts/WorkflowModels.h"

#include <type_traits>

static_assert(std::is_copy_constructible<aerosim::WorkflowViewV2>::value,
    "WorkflowViewV2 must remain a copyable value type");
static_assert(std::is_copy_assignable<aerosim::WorkflowViewV2>::value,
    "WorkflowViewV2 must remain a copy-assignable value type");

int compileWorkflowModelsHeader()
{
    aerosim::WorkflowStepViewV2 step;
    aerosim::WorkflowViewV2 view;
    view.steps.append(step);
    return view.steps.size() == 1 ? 0 : 1;
}
