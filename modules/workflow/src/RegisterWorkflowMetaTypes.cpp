#include "aerosim/workflow/ValidationTypes.h"

namespace aerosim {
namespace workflow {

void registerWorkflowMetaTypes()
{
    qRegisterMetaType<ValidationCompletion>(
        "aerosim::workflow::ValidationCompletion");
}

} // namespace workflow
} // namespace aerosim
