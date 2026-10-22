#pragma once

#include "aerosim/contracts/TaskPorts.h"

#include <memory>

namespace aerosim {

// A-owned composition root for shared services. GUI and CLI use the same class.
class ApplicationController final {
public:
    ApplicationController(std::unique_ptr<IWorkflowService> workflow,
                          std::unique_ptr<ITaskService> tasks);
    ~ApplicationController();

    ApplicationController(const ApplicationController &) = delete;
    ApplicationController &operator=(const ApplicationController &) = delete;

    bool initialize();
    void shutdown();
    IWorkflowService *workflow() const { return workflow_.get(); }
    ITaskService *tasks() const { return tasks_.get(); }

private:
    std::unique_ptr<IWorkflowService> workflow_;
    std::unique_ptr<ITaskService> tasks_;
    QMetaObject::Connection commandConnection_;
    QMetaObject::Connection eventConnection_;
    bool initialized_ = false;
};

} // namespace aerosim
