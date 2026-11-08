#include "aerosim/contracts/TaskPorts.h"

namespace {

// Relies on the default viewV2() contract: an implementation that does not
// override it must return Unsupported rather than fail to compile.
class MinimalWorkflow final : public aerosim::IWorkflowService {
public:
    using aerosim::IWorkflowService::IWorkflowService;

    aerosim::ValidationResult requestRun(const aerosim::RunRequest &) override
    {
        return aerosim::ValidationResult();
    }

    void onTaskEvent(const aerosim::TaskEvent &) override {}
};

} // namespace

int compileTaskPortsHeader()
{
    MinimalWorkflow workflow;
    return workflow.viewV2(aerosim::ScopeRef()).status()
                == aerosim::AccessStatus::Unsupported
        ? 0 : 1;
}
