#include "aerosim/contracts/TaskPorts.h"
#include "aerosim/contracts/Telemetry.h"
#include "aerosim/contracts/PostProcessRefs.h"
#include "aerosim/contracts/ConfigurationModels.h"
#include "aerosim/contracts/MeshModels.h"
#include "aerosim/contracts/ResourceModels.h"

#include <QMetaType>

namespace aerosim {

void registerContractMetaTypes()
{
    qRegisterMetaType<TaskCommand>("aerosim::TaskCommand");
    qRegisterMetaType<TaskEvent>("aerosim::TaskEvent");
    qRegisterMetaType<WorkflowViewV2>("aerosim::WorkflowViewV2");
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
    qRegisterMetaType<MeshRefV2>("aerosim::MeshRefV2");
    qRegisterMetaType<ParameterSnapshot>("aerosim::ParameterSnapshot");
    qRegisterMetaType<StateConfig>("aerosim::StateConfig");
    qRegisterMetaType<BatchPlanItem>("aerosim::BatchPlanItem");
    qRegisterMetaType<BatchPlan>("aerosim::BatchPlan");
    qRegisterMetaType<BatchPlanExecutionSnapshot>(
        "aerosim::BatchPlanExecutionSnapshot");
    qRegisterMetaType<TaskInputManifest>("aerosim::TaskInputManifest");
    qRegisterMetaType<TaskAttempt>("aerosim::TaskAttempt");
    qRegisterMetaType<BatchRun>("aerosim::BatchRun");
    qRegisterMetaType<TaskSnapshot>("aerosim::TaskSnapshot");
}

} // namespace aerosim
