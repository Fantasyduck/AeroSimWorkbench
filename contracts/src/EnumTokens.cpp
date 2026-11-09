#include "aerosim/contracts/EnumTokens.h"

#include <cstddef>

namespace aerosim {
namespace {

template <typename Enum>
struct EnumTokenEntry {
    Enum value;
    const char *token;
};

template <typename Enum, std::size_t Count>
QString toToken(Enum value, const EnumTokenEntry<Enum> (&entries)[Count])
{
    for (const EnumTokenEntry<Enum> &entry : entries) {
        if (entry.value == value)
            return QString::fromLatin1(entry.token);
    }
    return QString();
}

template <typename Enum, std::size_t Count>
AccessResult<Enum> fromToken(const QString &token, const char *typeName,
                             const EnumTokenEntry<Enum> (&entries)[Count])
{
    for (const EnumTokenEntry<Enum> &entry : entries) {
        if (token == QLatin1String(entry.token))
            return AccessResult<Enum>::success(entry.value);
    }
    return AccessResult<Enum>::failure(
        AccessStatus::InvalidArgument,
        QStringLiteral("contracts.enum.unknown_token"),
        QStringLiteral("Unknown %1 token: %2")
            .arg(QString::fromLatin1(typeName), token));
}

const EnumTokenEntry<AccessStatus> kAccessStatusTokens[] = {
    {AccessStatus::Ok, "ok"},
    {AccessStatus::NotFound, "not_found"},
    {AccessStatus::InvalidArgument, "invalid_argument"},
    {AccessStatus::Conflict, "conflict"},
    {AccessStatus::StaleRevision, "stale_revision"},
    {AccessStatus::Unavailable, "unavailable"},
    {AccessStatus::Unsupported, "unsupported"},
    {AccessStatus::IoError, "io_error"},
    {AccessStatus::InternalError, "internal_error"}
};

const EnumTokenEntry<IssueSeverity> kIssueSeverityTokens[] = {
    {IssueSeverity::Info, "info"},
    {IssueSeverity::Warning, "warning"},
    {IssueSeverity::Error, "error"}
};

const EnumTokenEntry<CompletionState> kCompletionStateTokens[] = {
    {CompletionState::NotStarted, "not_started"},
    {CompletionState::InProgress, "in_progress"},
    {CompletionState::Completed, "completed"}
};

const EnumTokenEntry<ValidationState> kValidationStateTokens[] = {
    {ValidationState::NotValidated, "not_validated"},
    {ValidationState::Valid, "valid"},
    {ValidationState::Invalid, "invalid"},
    {ValidationState::Stale, "stale"}
};

const EnumTokenEntry<TaskCommandType> kTaskCommandTypeTokens[] = {
    {TaskCommandType::Start, "start"},
    {TaskCommandType::Pause, "pause"},
    {TaskCommandType::Resume, "resume"},
    {TaskCommandType::Stop, "stop"},
    {TaskCommandType::Cancel, "cancel"}
};

const EnumTokenEntry<TaskEventType> kTaskEventTypeTokens[] = {
    {TaskEventType::Prepared, "prepared"},
    {TaskEventType::Started, "started"},
    {TaskEventType::StatusChanged, "status_changed"},
    {TaskEventType::ProgressUpdated, "progress_updated"},
    {TaskEventType::ResidualUpdated, "residual_updated"},
    {TaskEventType::LogGenerated, "log_generated"},
    {TaskEventType::Warning, "warning"},
    {TaskEventType::Failed, "failed"},
    {TaskEventType::Finished, "finished"},
    {TaskEventType::ResultGenerated, "result_generated"}
};

const EnumTokenEntry<TaskStatus> kTaskStatusTokens[] = {
    {TaskStatus::Unknown, "unknown"},
    {TaskStatus::Preparing, "preparing"},
    {TaskStatus::Running, "running"},
    {TaskStatus::Paused, "paused"},
    {TaskStatus::Finished, "finished"},
    {TaskStatus::Failed, "failed"},
    {TaskStatus::Cancelled, "cancelled"}
};

const EnumTokenEntry<LogLevel> kLogLevelTokens[] = {
    {LogLevel::Debug, "debug"},
    {LogLevel::Info, "info"},
    {LogLevel::Warning, "warning"},
    {LogLevel::Error, "error"}
};

const EnumTokenEntry<ResourceType> kResourceTypeTokens[] = {
    {ResourceType::Unknown, "unknown"},
    {ResourceType::ProjectFile, "project_file"},
    {ResourceType::ParameterSnapshot, "parameter_snapshot"},
    {ResourceType::TaskInputManifest, "task_input_manifest"},
    {ResourceType::BatchPlanSnapshot, "batch_plan_snapshot"},
    {ResourceType::Mesh, "mesh"},
    {ResourceType::Result, "result"},
    {ResourceType::Log, "log"},
    {ResourceType::Monitor, "monitor"},
    {ResourceType::Checkpoint, "checkpoint"},
    {ResourceType::Export, "export"}
};

const EnumTokenEntry<LocationKind> kLocationKindTokens[] = {
    {LocationKind::ProjectRelative, "project_relative"},
    {LocationKind::ExternalAbsolute, "external_absolute"},
    {LocationKind::Uri, "uri"}
};

const EnumTokenEntry<FingerprintStatus> kFingerprintStatusTokens[] = {
    {FingerprintStatus::NotComputed, "not_computed"},
    {FingerprintStatus::Computed, "computed"},
    {FingerprintStatus::Verified, "verified"}
};

const EnumTokenEntry<ParameterCategory> kParameterCategoryTokens[] = {
    {ParameterCategory::GoverningEquations, "governing_equations"},
    {ParameterCategory::BoundaryConditions, "boundary_conditions"},
    {ParameterCategory::MeshMotion, "mesh_motion"},
    {ParameterCategory::SolutionMethod, "solution_method"},
    {ParameterCategory::ReferenceValues, "reference_values"},
    {ParameterCategory::OutputControl, "output_control"},
    {ParameterCategory::Initialization, "initialization"}
};

const EnumTokenEntry<ParameterValueKind> kParameterValueKindTokens[] = {
    {ParameterValueKind::Unspecified, "unspecified"},
    {ParameterValueKind::Boolean, "boolean"},
    {ParameterValueKind::Integer, "integer"},
    {ParameterValueKind::Real, "real"},
    {ParameterValueKind::Text, "text"},
    {ParameterValueKind::Enumeration, "enumeration"}
};

const EnumTokenEntry<ExecutionState> kExecutionStateTokens[] = {
    {ExecutionState::Unknown, "unknown"},
    {ExecutionState::Created, "created"},
    {ExecutionState::Queued, "queued"},
    {ExecutionState::Preparing, "preparing"},
    {ExecutionState::Running, "running"},
    {ExecutionState::PauseRequested, "pause_requested"},
    {ExecutionState::Paused, "paused"},
    {ExecutionState::StopRequested, "stop_requested"},
    {ExecutionState::Cancelling, "cancelling"},
    {ExecutionState::Reconciling, "reconciling"},
    {ExecutionState::Finished, "finished"},
    {ExecutionState::Failed, "failed"},
    {ExecutionState::Stopped, "stopped"},
    {ExecutionState::Cancelled, "cancelled"}
};

const EnumTokenEntry<TaskOrigin> kTaskOriginTokens[] = {
    {TaskOrigin::Single, "single"},
    {TaskOrigin::Batch, "batch"}
};

const EnumTokenEntry<MeshRole> kMeshRoleTokens[] = {
    {MeshRole::Unknown, "unknown"},
    {MeshRole::Computation, "computation"},
    {MeshRole::Reference, "reference"},
    {MeshRole::Result, "result"},
    {MeshRole::Auxiliary, "auxiliary"}
};

const EnumTokenEntry<MeshTopology> kMeshTopologyTokens[] = {
    {MeshTopology::Unknown, "unknown"},
    {MeshTopology::Structured, "structured"},
    {MeshTopology::Unstructured, "unstructured"},
    {MeshTopology::Hybrid, "hybrid"},
    {MeshTopology::PointCloud, "point_cloud"}
};

const EnumTokenEntry<MeshRepresentation> kMeshRepresentationTokens[] = {
    {MeshRepresentation::Unknown, "unknown"},
    {MeshRepresentation::Surface, "surface"},
    {MeshRepresentation::SurfaceWithEdges, "surface_with_edges"},
    {MeshRepresentation::Wireframe, "wireframe"},
    {MeshRepresentation::Points, "points"}
};

const EnumTokenEntry<MeshTransformKind> kMeshTransformKindTokens[] = {
    {MeshTransformKind::Translation, "translation"},
    {MeshTransformKind::Rotation, "rotation"},
    {MeshTransformKind::Scaling, "scaling"},
    {MeshTransformKind::UnitConversion, "unit_conversion"},
    {MeshTransformKind::Composite, "composite"}
};

const EnumTokenEntry<LogStream> kLogStreamTokens[] = {
    {LogStream::Application, "application"},
    {LogStream::Stdout, "stdout"},
    {LogStream::Stderr, "stderr"}
};

const EnumTokenEntry<LogUpdateMode> kLogUpdateModeTokens[] = {
    {LogUpdateMode::AppendLine, "append_line"},
    {LogUpdateMode::AppendChunk, "append_chunk"},
    {LogUpdateMode::ReplaceCurrentLine, "replace_current_line"}
};

const EnumTokenEntry<MonitorQuantity> kMonitorQuantityTokens[] = {
    {MonitorQuantity::Residual, "residual"},
    {MonitorQuantity::Lift, "lift"},
    {MonitorQuantity::Drag, "drag"},
    {MonitorQuantity::Moment, "moment"},
    {MonitorQuantity::Coefficient, "coefficient"},
    {MonitorQuantity::Other, "other"}
};

const EnumTokenEntry<ResultKind> kResultKindTokens[] = {
    {ResultKind::Unknown, "unknown"},
    {ResultKind::Field, "field"},
    {ResultKind::Boundary, "boundary"},
    {ResultKind::Curve, "curve"},
    {ResultKind::Report, "report"},
    {ResultKind::Export, "export"}
};

const EnumTokenEntry<DatasetFieldAssociation> kDatasetFieldAssociationTokens[] = {
    {DatasetFieldAssociation::Unknown, "unknown"},
    {DatasetFieldAssociation::Point, "point"},
    {DatasetFieldAssociation::Cell, "cell"},
    {DatasetFieldAssociation::Boundary, "boundary"},
    {DatasetFieldAssociation::Global, "global"}
};

const EnumTokenEntry<SceneObjectKind> kSceneObjectKindTokens[] = {
    {SceneObjectKind::Unknown, "unknown"},
    {SceneObjectKind::SourceMesh, "source_mesh"},
    {SceneObjectKind::Slice, "slice"},
    {SceneObjectKind::IsoSurface, "iso_surface"},
    {SceneObjectKind::Contour, "contour"},
    {SceneObjectKind::Streamline, "streamline"},
    {SceneObjectKind::Pathline, "pathline"},
    {SceneObjectKind::Surface, "surface"},
    {SceneObjectKind::Curve, "curve"},
    {SceneObjectKind::Annotation, "annotation"}
};

} // namespace

#define AEROSIM_ENUM_TOKEN_FUNCTIONS(EnumType, functionStem, entries) \
    QString enumToken(EnumType value) { return toToken(value, entries); } \
    AccessResult<EnumType> functionStem##FromToken(const QString &token) \
    { \
        return fromToken(token, #EnumType, entries); \
    }

AEROSIM_ENUM_TOKEN_FUNCTIONS(AccessStatus, accessStatus, kAccessStatusTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(IssueSeverity, issueSeverity, kIssueSeverityTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(CompletionState, completionState, kCompletionStateTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(ValidationState, validationState, kValidationStateTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(TaskCommandType, taskCommandType, kTaskCommandTypeTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(TaskEventType, taskEventType, kTaskEventTypeTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(TaskStatus, taskStatus, kTaskStatusTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(LogLevel, logLevel, kLogLevelTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(ResourceType, resourceType, kResourceTypeTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(LocationKind, locationKind, kLocationKindTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(FingerprintStatus, fingerprintStatus, kFingerprintStatusTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(ParameterCategory, parameterCategory, kParameterCategoryTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(ParameterValueKind, parameterValueKind, kParameterValueKindTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(ExecutionState, executionState, kExecutionStateTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(TaskOrigin, taskOrigin, kTaskOriginTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(MeshRole, meshRole, kMeshRoleTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(MeshTopology, meshTopology, kMeshTopologyTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(MeshRepresentation, meshRepresentation, kMeshRepresentationTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(MeshTransformKind, meshTransformKind, kMeshTransformKindTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(LogStream, logStream, kLogStreamTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(LogUpdateMode, logUpdateMode, kLogUpdateModeTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(MonitorQuantity, monitorQuantity, kMonitorQuantityTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(ResultKind, resultKind, kResultKindTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(DatasetFieldAssociation, datasetFieldAssociation,
                             kDatasetFieldAssociationTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(SceneObjectKind, sceneObjectKind, kSceneObjectKindTokens)

#undef AEROSIM_ENUM_TOKEN_FUNCTIONS

} // namespace aerosim
