#include "aerosim/contracts/TaskPorts.h"

namespace {

class LegacyWorkflow final : public aerosim::IWorkflowService {
public:
    using aerosim::IWorkflowService::IWorkflowService;

    aerosim::WorkflowView view(const aerosim::ScopeRef &) const override
    {
        return aerosim::WorkflowView();
    }

    aerosim::ValidationResult requestRun(const aerosim::RunRequest &) override
    {
        return aerosim::ValidationResult();
    }

    void onTaskEvent(const aerosim::TaskEvent &) override {}
};

} // namespace

int compileTaskPortsHeader()
{
    LegacyWorkflow workflow;
    return workflow.viewV2(aerosim::ScopeRef()).status()
                == aerosim::AccessStatus::Unsupported
        ? 0 : 1;
}
