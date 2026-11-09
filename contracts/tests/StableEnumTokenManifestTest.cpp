#include "TestSupport.h"

#include "aerosim/contracts/EnumTokens.h"

#include <initializer_list>
#include <set>
#include <string>
#include <utility>

namespace {

template <typename Enum>
void checkMapping(
    aerosim_test::TestContext &context,
    QString (*toToken)(Enum),
    aerosim::AccessResult<Enum> (*fromToken)(const QString &),
    std::initializer_list<std::pair<Enum, const char *> > entries)
{
    std::set<std::string> uniqueTokens;
    for (const std::pair<Enum, const char *> &entry : entries) {
        const QString token = QString::fromLatin1(entry.second);
        AEROSIM_CHECK(context, toToken(entry.first) == token);
        const aerosim::AccessResult<Enum> parsed = fromToken(token);
        AEROSIM_CHECK(context, parsed.ok());
        AEROSIM_CHECK(context, parsed.hasValue());
        AEROSIM_CHECK(context, parsed.value() == entry.first);
        AEROSIM_CHECK(context, uniqueTokens.insert(entry.second).second);
    }

    AEROSIM_CHECK(context, toToken(static_cast<Enum>(-1)).isEmpty());
    const aerosim::AccessResult<Enum> unknown =
        fromToken(QStringLiteral("not_a_contract_token"));
    AEROSIM_CHECK(context, !unknown.ok());
    AEROSIM_CHECK(context, !unknown.hasValue());
    AEROSIM_CHECK(context,
        unknown.status() == aerosim::AccessStatus::InvalidArgument);
    AEROSIM_CHECK(context,
        unknown.errorCode() == QStringLiteral("contracts.enum.unknown_token"));
}

} // namespace

int main()
{
    aerosim_test::TestContext context;

    checkMapping<aerosim::AccessStatus>(context, &aerosim::enumToken,
        &aerosim::accessStatusFromToken,
        {{aerosim::AccessStatus::Ok, "ok"},
         {aerosim::AccessStatus::NotFound, "not_found"},
         {aerosim::AccessStatus::InvalidArgument, "invalid_argument"},
         {aerosim::AccessStatus::Conflict, "conflict"},
         {aerosim::AccessStatus::StaleRevision, "stale_revision"},
         {aerosim::AccessStatus::Unavailable, "unavailable"},
         {aerosim::AccessStatus::Unsupported, "unsupported"},
         {aerosim::AccessStatus::IoError, "io_error"},
         {aerosim::AccessStatus::InternalError, "internal_error"}});

    checkMapping<aerosim::IssueSeverity>(context, &aerosim::enumToken,
        &aerosim::issueSeverityFromToken,
        {{aerosim::IssueSeverity::Info, "info"},
         {aerosim::IssueSeverity::Warning, "warning"},
         {aerosim::IssueSeverity::Error, "error"}});

    checkMapping<aerosim::CompletionState>(context, &aerosim::enumToken,
        &aerosim::completionStateFromToken,
        {{aerosim::CompletionState::NotStarted, "not_started"},
         {aerosim::CompletionState::InProgress, "in_progress"},
         {aerosim::CompletionState::Completed, "completed"}});

    checkMapping<aerosim::ValidationState>(context, &aerosim::enumToken,
        &aerosim::validationStateFromToken,
        {{aerosim::ValidationState::NotValidated, "not_validated"},
         {aerosim::ValidationState::Valid, "valid"},
         {aerosim::ValidationState::Invalid, "invalid"},
         {aerosim::ValidationState::Stale, "stale"}});

    checkMapping<aerosim::TaskCommandType>(context, &aerosim::enumToken,
        &aerosim::taskCommandTypeFromToken,
        {{aerosim::TaskCommandType::Start, "start"},
         {aerosim::TaskCommandType::Pause, "pause"},
         {aerosim::TaskCommandType::Resume, "resume"},
         {aerosim::TaskCommandType::Stop, "stop"},
         {aerosim::TaskCommandType::Cancel, "cancel"}});

    checkMapping<aerosim::TaskEventType>(context, &aerosim::enumToken,
        &aerosim::taskEventTypeFromToken,
        {{aerosim::TaskEventType::Prepared, "prepared"},
         {aerosim::TaskEventType::Started, "started"},
         {aerosim::TaskEventType::StatusChanged, "status_changed"},
         {aerosim::TaskEventType::ProgressUpdated, "progress_updated"},
         {aerosim::TaskEventType::ResidualUpdated, "residual_updated"},
         {aerosim::TaskEventType::LogGenerated, "log_generated"},
         {aerosim::TaskEventType::Warning, "warning"},
         {aerosim::TaskEventType::Failed, "failed"},
         {aerosim::TaskEventType::Finished, "finished"},
         {aerosim::TaskEventType::ResultGenerated, "result_generated"}});

    checkMapping<aerosim::TaskStatus>(context, &aerosim::enumToken,
        &aerosim::taskStatusFromToken,
        {{aerosim::TaskStatus::Unknown, "unknown"},
         {aerosim::TaskStatus::Preparing, "preparing"},
         {aerosim::TaskStatus::Running, "running"},
         {aerosim::TaskStatus::Paused, "paused"},
         {aerosim::TaskStatus::Finished, "finished"},
         {aerosim::TaskStatus::Failed, "failed"},
         {aerosim::TaskStatus::Cancelled, "cancelled"}});

    checkMapping<aerosim::LogLevel>(context, &aerosim::enumToken,
        &aerosim::logLevelFromToken,
        {{aerosim::LogLevel::Debug, "debug"},
         {aerosim::LogLevel::Info, "info"},
         {aerosim::LogLevel::Warning, "warning"},
         {aerosim::LogLevel::Error, "error"}});

    checkMapping<aerosim::ResourceType>(context, &aerosim::enumToken,
        &aerosim::resourceTypeFromToken,
        {{aerosim::ResourceType::Unknown, "unknown"},
         {aerosim::ResourceType::ProjectFile, "project_file"},
         {aerosim::ResourceType::ParameterSnapshot, "parameter_snapshot"},
         {aerosim::ResourceType::TaskInputManifest, "task_input_manifest"},
         {aerosim::ResourceType::BatchPlanSnapshot, "batch_plan_snapshot"},
         {aerosim::ResourceType::Mesh, "mesh"},
         {aerosim::ResourceType::Result, "result"},
         {aerosim::ResourceType::Log, "log"},
         {aerosim::ResourceType::Monitor, "monitor"},
         {aerosim::ResourceType::Checkpoint, "checkpoint"},
         {aerosim::ResourceType::Export, "export"}});

    checkMapping<aerosim::LocationKind>(context, &aerosim::enumToken,
        &aerosim::locationKindFromToken,
        {{aerosim::LocationKind::ProjectRelative, "project_relative"},
         {aerosim::LocationKind::ExternalAbsolute, "external_absolute"},
         {aerosim::LocationKind::Uri, "uri"}});

    checkMapping<aerosim::FingerprintStatus>(context, &aerosim::enumToken,
        &aerosim::fingerprintStatusFromToken,
        {{aerosim::FingerprintStatus::NotComputed, "not_computed"},
         {aerosim::FingerprintStatus::Computed, "computed"},
         {aerosim::FingerprintStatus::Verified, "verified"}});

    checkMapping<aerosim::ParameterCategory>(context, &aerosim::enumToken,
        &aerosim::parameterCategoryFromToken,
        {{aerosim::ParameterCategory::GoverningEquations, "governing_equations"},
         {aerosim::ParameterCategory::BoundaryConditions, "boundary_conditions"},
         {aerosim::ParameterCategory::MeshMotion, "mesh_motion"},
         {aerosim::ParameterCategory::SolutionMethod, "solution_method"},
         {aerosim::ParameterCategory::ReferenceValues, "reference_values"},
         {aerosim::ParameterCategory::OutputControl, "output_control"},
         {aerosim::ParameterCategory::Initialization, "initialization"}});

    checkMapping<aerosim::ParameterValueKind>(context, &aerosim::enumToken,
        &aerosim::parameterValueKindFromToken,
        {{aerosim::ParameterValueKind::Unspecified, "unspecified"},
         {aerosim::ParameterValueKind::Boolean, "boolean"},
         {aerosim::ParameterValueKind::Integer, "integer"},
         {aerosim::ParameterValueKind::Real, "real"},
         {aerosim::ParameterValueKind::Text, "text"},
         {aerosim::ParameterValueKind::Enumeration, "enumeration"}});

    checkMapping<aerosim::ExecutionState>(context, &aerosim::enumToken,
        &aerosim::executionStateFromToken,
        {{aerosim::ExecutionState::Unknown, "unknown"},
         {aerosim::ExecutionState::Created, "created"},
         {aerosim::ExecutionState::Queued, "queued"},
         {aerosim::ExecutionState::Preparing, "preparing"},
         {aerosim::ExecutionState::Running, "running"},
         {aerosim::ExecutionState::PauseRequested, "pause_requested"},
         {aerosim::ExecutionState::Paused, "paused"},
         {aerosim::ExecutionState::StopRequested, "stop_requested"},
         {aerosim::ExecutionState::Cancelling, "cancelling"},
         {aerosim::ExecutionState::Reconciling, "reconciling"},
         {aerosim::ExecutionState::Finished, "finished"},
         {aerosim::ExecutionState::Failed, "failed"},
         {aerosim::ExecutionState::Stopped, "stopped"},
         {aerosim::ExecutionState::Cancelled, "cancelled"}});

    checkMapping<aerosim::TaskOrigin>(context, &aerosim::enumToken,
        &aerosim::taskOriginFromToken,
        {{aerosim::TaskOrigin::Single, "single"},
         {aerosim::TaskOrigin::Batch, "batch"}});

    checkMapping<aerosim::MeshRole>(context, &aerosim::enumToken,
        &aerosim::meshRoleFromToken,
        {{aerosim::MeshRole::Unknown, "unknown"},
         {aerosim::MeshRole::Computation, "computation"},
         {aerosim::MeshRole::Reference, "reference"},
         {aerosim::MeshRole::Result, "result"},
         {aerosim::MeshRole::Auxiliary, "auxiliary"}});

    checkMapping<aerosim::MeshTopology>(context, &aerosim::enumToken,
        &aerosim::meshTopologyFromToken,
        {{aerosim::MeshTopology::Unknown, "unknown"},
         {aerosim::MeshTopology::Structured, "structured"},
         {aerosim::MeshTopology::Unstructured, "unstructured"},
         {aerosim::MeshTopology::Hybrid, "hybrid"},
         {aerosim::MeshTopology::PointCloud, "point_cloud"}});

    checkMapping<aerosim::MeshRepresentation>(context, &aerosim::enumToken,
        &aerosim::meshRepresentationFromToken,
        {{aerosim::MeshRepresentation::Unknown, "unknown"},
         {aerosim::MeshRepresentation::Surface, "surface"},
         {aerosim::MeshRepresentation::SurfaceWithEdges, "surface_with_edges"},
         {aerosim::MeshRepresentation::Wireframe, "wireframe"},
         {aerosim::MeshRepresentation::Points, "points"}});

    checkMapping<aerosim::MeshTransformKind>(context, &aerosim::enumToken,
        &aerosim::meshTransformKindFromToken,
        {{aerosim::MeshTransformKind::Translation, "translation"},
         {aerosim::MeshTransformKind::Rotation, "rotation"},
         {aerosim::MeshTransformKind::Scaling, "scaling"},
         {aerosim::MeshTransformKind::UnitConversion, "unit_conversion"},
         {aerosim::MeshTransformKind::Composite, "composite"}});

    checkMapping<aerosim::LogStream>(context, &aerosim::enumToken,
        &aerosim::logStreamFromToken,
        {{aerosim::LogStream::Application, "application"},
         {aerosim::LogStream::Stdout, "stdout"},
         {aerosim::LogStream::Stderr, "stderr"}});

    checkMapping<aerosim::LogUpdateMode>(context, &aerosim::enumToken,
        &aerosim::logUpdateModeFromToken,
        {{aerosim::LogUpdateMode::AppendLine, "append_line"},
         {aerosim::LogUpdateMode::AppendChunk, "append_chunk"},
         {aerosim::LogUpdateMode::ReplaceCurrentLine, "replace_current_line"}});

    checkMapping<aerosim::MonitorQuantity>(context, &aerosim::enumToken,
        &aerosim::monitorQuantityFromToken,
        {{aerosim::MonitorQuantity::Residual, "residual"},
         {aerosim::MonitorQuantity::Lift, "lift"},
         {aerosim::MonitorQuantity::Drag, "drag"},
         {aerosim::MonitorQuantity::Moment, "moment"},
         {aerosim::MonitorQuantity::Coefficient, "coefficient"},
         {aerosim::MonitorQuantity::Other, "other"}});

    checkMapping<aerosim::ResultKind>(context, &aerosim::enumToken,
        &aerosim::resultKindFromToken,
        {{aerosim::ResultKind::Unknown, "unknown"},
         {aerosim::ResultKind::Field, "field"},
         {aerosim::ResultKind::Boundary, "boundary"},
         {aerosim::ResultKind::Curve, "curve"},
         {aerosim::ResultKind::Report, "report"},
         {aerosim::ResultKind::Export, "export"}});

    checkMapping<aerosim::DatasetFieldAssociation>(context, &aerosim::enumToken,
        &aerosim::datasetFieldAssociationFromToken,
        {{aerosim::DatasetFieldAssociation::Unknown, "unknown"},
         {aerosim::DatasetFieldAssociation::Point, "point"},
         {aerosim::DatasetFieldAssociation::Cell, "cell"},
         {aerosim::DatasetFieldAssociation::Boundary, "boundary"},
         {aerosim::DatasetFieldAssociation::Global, "global"}});

    checkMapping<aerosim::SceneObjectKind>(context, &aerosim::enumToken,
        &aerosim::sceneObjectKindFromToken,
        {{aerosim::SceneObjectKind::Unknown, "unknown"},
         {aerosim::SceneObjectKind::SourceMesh, "source_mesh"},
         {aerosim::SceneObjectKind::Slice, "slice"},
         {aerosim::SceneObjectKind::IsoSurface, "iso_surface"},
         {aerosim::SceneObjectKind::Contour, "contour"},
         {aerosim::SceneObjectKind::Streamline, "streamline"},
         {aerosim::SceneObjectKind::Pathline, "pathline"},
         {aerosim::SceneObjectKind::Surface, "surface"},
         {aerosim::SceneObjectKind::Curve, "curve"},
         {aerosim::SceneObjectKind::Annotation, "annotation"}});

    return context.result();
}
