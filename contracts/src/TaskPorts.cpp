#include "aerosim/contracts/TaskPorts.h"

namespace aerosim {

AccessResult<WorkflowViewV2>
IWorkflowService::viewV2(const ScopeRef &scope) const
{
    Q_UNUSED(scope);
    return AccessResult<WorkflowViewV2>::failure(
        AccessStatus::Unsupported,
        QStringLiteral("workflow.v2.unsupported"),
        QStringLiteral("implementation does not provide Workflow V2"));
}

} // namespace aerosim
