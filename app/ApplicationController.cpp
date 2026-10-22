#include "ApplicationController.h"

#include <QCoreApplication>
#include <QThread>

namespace aerosim {

ApplicationController::ApplicationController(std::unique_ptr<IWorkflowService> workflow,
                                             std::unique_ptr<ITaskService> tasks)
    : workflow_(std::move(workflow)), tasks_(std::move(tasks))
{
}

ApplicationController::~ApplicationController()
{
    shutdown();
}

bool ApplicationController::initialize()
{
    if (initialized_)
        return true;
    if (!QCoreApplication::instance() || !workflow_ || !tasks_)
        return false;
    // Service facades live in the entry thread. F may own worker components and
    // QProcess internally without moving this facade or blocking its event loop.
    if (workflow_->thread() != QThread::currentThread() ||
        tasks_->thread() != QThread::currentThread())
        return false;

    registerContractMetaTypes();
    commandConnection_ = QObject::connect(workflow_.get(), &IWorkflowService::taskCommandRequested,
                                          tasks_.get(), &ITaskService::handleTaskCommand,
                                          Qt::QueuedConnection);
    eventConnection_ = QObject::connect(tasks_.get(), &ITaskService::taskEventOccurred,
                                        workflow_.get(), &IWorkflowService::onTaskEvent,
                                        Qt::QueuedConnection);
    if (!commandConnection_ || !eventConnection_) {
        shutdown();
        return false;
    }
    initialized_ = true;
    return true;
}

void ApplicationController::shutdown()
{
    QObject::disconnect(commandConnection_);
    QObject::disconnect(eventConnection_);
    commandConnection_ = {};
    eventConnection_ = {};
    initialized_ = false;
}

} // namespace aerosim
