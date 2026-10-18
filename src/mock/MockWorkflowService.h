#pragma once

#include "aerosim/contracts/TaskPorts.h"

namespace aerosim {

// PLACEHOLDER for G. It deliberately performs no professional validation.
class MockWorkflowService final : public IWorkflowService {
    Q_OBJECT
public:
    using IWorkflowService::IWorkflowService;
    WorkflowView view(const ScopeRef &scope) const override;
    ValidationResult requestRun(const RunRequest &request) override;

public slots:
    void onTaskEvent(const TaskEvent &event) override;

signals:
    void mockEventObserved(const aerosim::TaskEvent &event);

private:
    quint64 viewRevision_ = 0;
};

} // namespace aerosim
