#include "aerosim/contracts/TaskPorts.h"
#include "aerosim/contracts/Telemetry.h"

#include <QMetaType>

namespace aerosim {

void registerContractMetaTypes()
{
    qRegisterMetaType<TaskCommand>("aerosim::TaskCommand");
    qRegisterMetaType<TaskEvent>("aerosim::TaskEvent");
    qRegisterMetaType<WorkflowView>("aerosim::WorkflowView");
    qRegisterMetaType<LogEntry>("aerosim::LogEntry");
    qRegisterMetaType<ErrorInfo>("aerosim::ErrorInfo");
    qRegisterMetaType<MonitorSample>("aerosim::MonitorSample");
}

} // namespace aerosim
