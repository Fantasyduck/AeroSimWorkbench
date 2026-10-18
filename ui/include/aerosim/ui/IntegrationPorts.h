#pragma once

#include "aerosim/contracts/DataAccess.h"
#include "aerosim/contracts/TaskPorts.h"

class QWidget;

namespace aerosim {

// B implements these on its SARibbonMainWindow. The host retains QWidget ownership
// after successful insertion; A/C/E must not delete an inserted widget.
enum class WidgetSlot { Viewport, Log, Error, Monitor };

struct ServiceBindings {
    IWorkflowService *workflow = nullptr;
    ITaskService *tasks = nullptr;
    IProjectAccess *projects = nullptr;
    IParameterAccess *parameters = nullptr;
    IMeshAccess *meshes = nullptr;
    IResultAccess *results = nullptr;
};

class IWidgetHost {
public:
    virtual ~IWidgetHost() = default;
    virtual QWidget *windowWidget() = 0;
    virtual bool insertWidget(WidgetSlot slot, QWidget *widget) = 0;
    virtual void bindServices(const ServiceBindings &services) = 0;
};

} // namespace aerosim
