#pragma once

#include "aerosim/contracts/TaskPorts.h"

#include <QHash>

namespace aerosim {

// PLACEHOLDER for F. No solver, process, persistence, or scheduling occurs.
class MockTaskService final : public ITaskService {
    Q_OBJECT
public:
    using ITaskService::ITaskService;
    TaskRecord record(const QString &taskId) const override;

public slots:
    void handleTaskCommand(const TaskCommand &command) override;

private:
    QHash<QString, TaskRecord> records_;
};

} // namespace aerosim
