#pragma once

#include "aerosim/contracts/TaskModels.h"
#include "aerosim/contracts/WorkflowModels.h"

#include <QObject>
#include <QMetaType>

namespace aerosim {

// G owns the implementation. It validates before emitting a command.
class IWorkflowService : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~IWorkflowService() override = default;
    virtual WorkflowView view(const ScopeRef &scope) const = 0;
    virtual ValidationResult requestRun(const RunRequest &request) = 0;

public slots:
    virtual void onTaskEvent(const aerosim::TaskEvent &event) = 0;

signals:
    void taskCommandRequested(const aerosim::TaskCommand &command);
    void workflowViewChanged(const aerosim::WorkflowView &view);
};

// F owns the implementation and TaskRecord persistence.
class ITaskService : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~ITaskService() override = default;
    virtual TaskRecord record(const QString &taskId) const = 0;

public slots:
    virtual void handleTaskCommand(const aerosim::TaskCommand &command) = 0;

signals:
    void taskEventOccurred(const aerosim::TaskEvent &event);
};

void registerContractMetaTypes();

} // namespace aerosim
