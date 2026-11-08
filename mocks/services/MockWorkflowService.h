#pragma once

#include "aerosim/contracts/TaskPorts.h"
#include "aerosim/workflow/WorkflowService.h"

namespace aerosim {

// PLACEHOLDER for G. It deliberately performs no professional validation.
class MockWorkflowService final : public IWorkflowService {
    Q_OBJECT
public:
    explicit MockWorkflowService(QObject *parent = nullptr);
    AccessResult<WorkflowViewV2> viewV2(const ScopeRef &scope) const override;
    ValidationResult requestRun(const RunRequest &request) override;
    workflow::WorkflowService &controlledService()
    {
        // Test-controlled mode supplies its own complete definition/facts and
        // must not be overwritten by the legacy CLI bootstrap.
        bootstrapped_ = true;
        return engine_;
    }

public slots:
    void onTaskEvent(const TaskEvent &event) override;

signals:
    void mockEventObserved(const aerosim::TaskEvent &event);

private:
    void ensureBootstrap(const RunRequest &request);
    workflow::WorkflowService engine_;
    bool bootstrapped_ = false;
};

} // namespace aerosim
