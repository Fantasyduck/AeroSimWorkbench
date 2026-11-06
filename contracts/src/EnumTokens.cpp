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

const EnumTokenEntry<StepState> kStepStateTokens[] = {
    {StepState::Pending, "pending"},
    {StepState::NeedsValidation, "needs_validation"},
    {StepState::Complete, "complete"},
    {StepState::Blocked, "blocked"}
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

} // namespace

#define AEROSIM_ENUM_TOKEN_FUNCTIONS(EnumType, functionStem, entries) \
    QString enumToken(EnumType value) { return toToken(value, entries); } \
    AccessResult<EnumType> functionStem##FromToken(const QString &token) \
    { \
        return fromToken(token, #EnumType, entries); \
    }

AEROSIM_ENUM_TOKEN_FUNCTIONS(AccessStatus, accessStatus, kAccessStatusTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(IssueSeverity, issueSeverity, kIssueSeverityTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(StepState, stepState, kStepStateTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(TaskCommandType, taskCommandType, kTaskCommandTypeTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(TaskEventType, taskEventType, kTaskEventTypeTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(TaskStatus, taskStatus, kTaskStatusTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(LogLevel, logLevel, kLogLevelTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(ResourceType, resourceType, kResourceTypeTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(LocationKind, locationKind, kLocationKindTokens)
AEROSIM_ENUM_TOKEN_FUNCTIONS(FingerprintStatus, fingerprintStatus, kFingerprintStatusTokens)

#undef AEROSIM_ENUM_TOKEN_FUNCTIONS

} // namespace aerosim
