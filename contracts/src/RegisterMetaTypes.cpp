#include "aerosim/contracts/TaskPorts.h"
#include "aerosim/contracts/Telemetry.h"
#include "aerosim/contracts/PostProcessRefs.h"
#include "aerosim/contracts/ConfigurationModels.h"
#include "aerosim/contracts/MeshModels.h"
#include "aerosim/contracts/ResourceModels.h"
#include "aerosim/contracts/ResultModels.h"

#include <QMetaType>

namespace aerosim {

void registerContractMetaTypes()
{
    qRegisterMetaType<TaskCommand>("aerosim::TaskCommand");
    qRegisterMetaType<TaskEvent>("aerosim::TaskEvent");
    qRegisterMetaType<WorkflowViewV2>("aerosim::WorkflowViewV2");
    qRegisterMetaType<ErrorInfo>("aerosim::ErrorInfo");
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
    qRegisterMetaType<MeshVector3>("aerosim::MeshVector3");
    qRegisterMetaType<RgbaColor>("aerosim::RgbaColor");
    qRegisterMetaType<MeshDescriptor>("aerosim::MeshDescriptor");
    qRegisterMetaType<MeshTransform>("aerosim::MeshTransform");
    qRegisterMetaType<MeshPresentation>("aerosim::MeshPresentation");
    qRegisterMetaType<MeshCollection>("aerosim::MeshCollection");
    qRegisterMetaType<LogRecordRef>("aerosim::LogRecordRef");
    qRegisterMetaType<LogEntryV2>("aerosim::LogEntryV2");
    qRegisterMetaType<LogFragment>("aerosim::LogFragment");
    qRegisterMetaType<MonitorSeriesRef>("aerosim::MonitorSeriesRef");
    qRegisterMetaType<MonitorSampleV2>("aerosim::MonitorSampleV2");
    qRegisterMetaType<MonitorSeries>("aerosim::MonitorSeries");
    qRegisterMetaType<MonitorBatch>("aerosim::MonitorBatch");
    qRegisterMetaType<ResultDescriptor>("aerosim::ResultDescriptor");
    qRegisterMetaType<DatasetDescriptor>("aerosim::DatasetDescriptor");
    qRegisterMetaType<SceneObjectDescriptor>("aerosim::SceneObjectDescriptor");
    qRegisterMetaType<PostProcessScene>("aerosim::PostProcessScene");
}

} // namespace aerosim
