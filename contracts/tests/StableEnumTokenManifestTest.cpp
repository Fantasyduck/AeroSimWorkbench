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

    checkMapping<aerosim::StepState>(context, &aerosim::enumToken,
        &aerosim::stepStateFromToken,
        {{aerosim::StepState::Pending, "pending"},
         {aerosim::StepState::NeedsValidation, "needs_validation"},
         {aerosim::StepState::Complete, "complete"},
         {aerosim::StepState::Blocked, "blocked"}});

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

    return context.result();
}
