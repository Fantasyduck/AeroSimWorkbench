#pragma once

#include "aerosim/contracts/AccessResult.h"
#include "aerosim/contracts/ResourceModels.h"
#include "aerosim/contracts/TaskModels.h"
#include "aerosim/contracts/Telemetry.h"
#include "aerosim/contracts/WorkflowModels.h"

#include <QString>

namespace aerosim {

// Unknown enum values serialize to an empty string. Unknown input tokens return
// AccessStatus::InvalidArgument and never silently select a default value.
QString enumToken(AccessStatus value);
QString enumToken(IssueSeverity value);
QString enumToken(StepState value);
QString enumToken(TaskCommandType value);
QString enumToken(TaskEventType value);
QString enumToken(TaskStatus value);
QString enumToken(LogLevel value);
QString enumToken(ResourceType value);
QString enumToken(LocationKind value);
QString enumToken(FingerprintStatus value);

AccessResult<AccessStatus> accessStatusFromToken(const QString &token);
AccessResult<IssueSeverity> issueSeverityFromToken(const QString &token);
AccessResult<StepState> stepStateFromToken(const QString &token);
AccessResult<TaskCommandType> taskCommandTypeFromToken(const QString &token);
AccessResult<TaskEventType> taskEventTypeFromToken(const QString &token);
AccessResult<TaskStatus> taskStatusFromToken(const QString &token);
AccessResult<LogLevel> logLevelFromToken(const QString &token);
AccessResult<ResourceType> resourceTypeFromToken(const QString &token);
AccessResult<LocationKind> locationKindFromToken(const QString &token);
AccessResult<FingerprintStatus> fingerprintStatusFromToken(const QString &token);

} // namespace aerosim
