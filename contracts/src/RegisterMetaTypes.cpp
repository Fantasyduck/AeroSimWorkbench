#include "aerosim/contracts/TaskPorts.h"
#include "aerosim/contracts/Telemetry.h"
#include "aerosim/contracts/PostProcessRefs.h"
#include "aerosim/contracts/ResourceModels.h"

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
    qRegisterMetaType<DatasetRef>("aerosim::DatasetRef");
    qRegisterMetaType<SceneObjectRef>("aerosim::SceneObjectRef");
    qRegisterMetaType<StateRef>("aerosim::StateRef");
    qRegisterMetaType<BatchPlanRef>("aerosim::BatchPlanRef");
    qRegisterMetaType<BatchRunRef>("aerosim::BatchRunRef");
    qRegisterMetaType<TaskRef>("aerosim::TaskRef");
    qRegisterMetaType<AttemptRef>("aerosim::AttemptRef");
    qRegisterMetaType<ResourceLocation>("aerosim::ResourceLocation");
    qRegisterMetaType<ContentFingerprint>("aerosim::ContentFingerprint");
    qRegisterMetaType<ResourceRefV2>("aerosim::ResourceRefV2");
}

} // namespace aerosim
