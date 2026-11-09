#pragma once

#include "aerosim/contracts/AccessResult.h"
#include "aerosim/contracts/ConfigurationModels.h"
#include "aerosim/contracts/ExecutionModels.h"
#include "aerosim/contracts/MeshModels.h"
#include "aerosim/contracts/ResourceModels.h"
#include "aerosim/contracts/ResultModels.h"
#include "aerosim/contracts/TaskModels.h"
#include "aerosim/contracts/Telemetry.h"
#include "aerosim/contracts/WorkflowModels.h"

#include <QString>

namespace aerosim {

// Unknown enum values serialize to an empty string. Unknown input tokens return
// AccessStatus::InvalidArgument and never silently select a default value.
QString enumToken(AccessStatus value);
QString enumToken(IssueSeverity value);
QString enumToken(CompletionState value);
QString enumToken(ValidationState value);
QString enumToken(TaskCommandType value);
QString enumToken(TaskEventType value);
QString enumToken(TaskStatus value);
QString enumToken(LogLevel value);
QString enumToken(ResourceType value);
QString enumToken(LocationKind value);
QString enumToken(FingerprintStatus value);
QString enumToken(ParameterCategory value);
QString enumToken(ParameterValueKind value);
QString enumToken(ExecutionState value);
QString enumToken(TaskOrigin value);
QString enumToken(MeshRole value);
QString enumToken(MeshTopology value);
QString enumToken(MeshRepresentation value);
QString enumToken(MeshTransformKind value);
QString enumToken(LogStream value);
QString enumToken(LogUpdateMode value);
QString enumToken(MonitorQuantity value);
QString enumToken(ResultKind value);
QString enumToken(DatasetFieldAssociation value);
QString enumToken(SceneObjectKind value);

AccessResult<AccessStatus> accessStatusFromToken(const QString &token);
AccessResult<IssueSeverity> issueSeverityFromToken(const QString &token);
AccessResult<CompletionState> completionStateFromToken(const QString &token);
AccessResult<ValidationState> validationStateFromToken(const QString &token);
AccessResult<TaskCommandType> taskCommandTypeFromToken(const QString &token);
AccessResult<TaskEventType> taskEventTypeFromToken(const QString &token);
AccessResult<TaskStatus> taskStatusFromToken(const QString &token);
AccessResult<LogLevel> logLevelFromToken(const QString &token);
AccessResult<ResourceType> resourceTypeFromToken(const QString &token);
AccessResult<LocationKind> locationKindFromToken(const QString &token);
AccessResult<FingerprintStatus> fingerprintStatusFromToken(const QString &token);
AccessResult<ParameterCategory> parameterCategoryFromToken(const QString &token);
AccessResult<ParameterValueKind> parameterValueKindFromToken(const QString &token);
AccessResult<ExecutionState> executionStateFromToken(const QString &token);
AccessResult<TaskOrigin> taskOriginFromToken(const QString &token);
AccessResult<MeshRole> meshRoleFromToken(const QString &token);
AccessResult<MeshTopology> meshTopologyFromToken(const QString &token);
AccessResult<MeshRepresentation> meshRepresentationFromToken(const QString &token);
AccessResult<MeshTransformKind> meshTransformKindFromToken(const QString &token);
AccessResult<LogStream> logStreamFromToken(const QString &token);
AccessResult<LogUpdateMode> logUpdateModeFromToken(const QString &token);
AccessResult<MonitorQuantity> monitorQuantityFromToken(const QString &token);
AccessResult<ResultKind> resultKindFromToken(const QString &token);
AccessResult<DatasetFieldAssociation> datasetFieldAssociationFromToken(const QString &token);
AccessResult<SceneObjectKind> sceneObjectKindFromToken(const QString &token);

} // namespace aerosim
