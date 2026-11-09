#include "TestSupport.h"

#include "aerosim/contracts/EnumTokens.h"

#include <limits>

namespace {

aerosim::ResourceRefV2 verifiedResource(const QString &resourceId,
                                        aerosim::ResourceType type)
{
    aerosim::ResourceRefV2 resource;
    resource.projectId = QStringLiteral("project-a");
    resource.resourceId = resourceId;
    resource.revision = 1;
    resource.type = type;
    resource.location = aerosim::ResourceLocation::projectRelative(
        QStringLiteral("runtime/output.log"));
    resource.fingerprint.status = aerosim::FingerprintStatus::Verified;
    resource.fingerprint.algorithm = QStringLiteral("sha256");
    resource.fingerprint.digest = QStringLiteral("cafebabe");
    return resource;
}

aerosim::TaskRef taskRef()
{
    aerosim::TaskRef task;
    task.projectId = QStringLiteral("project-a");
    task.taskId = QStringLiteral("task-a");
    return task;
}

aerosim::AttemptRef attemptRef(quint32 index = 1)
{
    aerosim::AttemptRef attempt;
    attempt.task = taskRef();
    attempt.attemptId = QStringLiteral("attempt-%1").arg(index);
    return attempt;
}

aerosim::LogEntryV2 logEntry(quint64 sequence, aerosim::LogStream stream,
                            aerosim::LogUpdateMode updateMode)
{
    aerosim::LogEntryV2 entry;
    entry.projectId = QStringLiteral("project-a");
    entry.task = taskRef();
    entry.attemptId = QStringLiteral("attempt-1");
    entry.sequence = sequence;
    entry.timestamp = QDateTime::currentDateTimeUtc();
    entry.stream = stream;
    entry.updateMode = updateMode;
    entry.source = QStringLiteral("solver");
    entry.text = QStringLiteral("iteration output");
    return entry;
}

aerosim::MonitorSampleV2 sample(const QString &seriesId,
                               aerosim::MonitorQuantity quantity,
                               const QString &metricKey,
                               const QString &unit,
                               double value,
                               quint64 step,
                               quint64 sequence = 0)
{
    aerosim::MonitorSampleV2 record;
    record.projectId = QStringLiteral("project-a");
    record.task = taskRef();
    record.attemptId = QStringLiteral("attempt-1");
    record.seriesId = seriesId;
    record.quantity = quantity;
    record.metricKey = metricKey;
    record.unit = unit;
    record.value = value;
    record.step = step;
    record.sequence = sequence;
    return record;
}

void checkLogEntry(aerosim_test::TestContext &context)
{
    aerosim::LogEntryV2 entry;
    AEROSIM_CHECK(context, !entry.isValid());

    entry = logEntry(1, aerosim::LogStream::Stdout, aerosim::LogUpdateMode::AppendLine);
    AEROSIM_CHECK(context, entry.isValid());
    AEROSIM_CHECK(context, entry.stream == aerosim::LogStream::Stdout);
    AEROSIM_CHECK(context, entry.updateMode == aerosim::LogUpdateMode::AppendLine);

    // Sequence 0 is not a persisted record.
    entry.sequence = 0;
    AEROSIM_CHECK(context, !entry.isValid());
    entry.sequence = 5;

    // An attempt id is mandatory so output never mixes across retries.
    entry.attemptId.clear();
    AEROSIM_CHECK(context, !entry.isValid());
    entry.attemptId = QStringLiteral("attempt-1");

    // A record must not claim a different project than its task.
    entry.projectId = QStringLiteral("project-b");
    AEROSIM_CHECK(context, !entry.isValid());
}

void checkLogFragmentAndModes(aerosim_test::TestContext &context)
{
    // Display fragments are ephemeral and distinct from persisted records.
    aerosim::LogFragment fragment;
    AEROSIM_CHECK(context, !fragment.isValid());
    fragment.task = taskRef();
    fragment.attemptId = QStringLiteral("attempt-1");
    fragment.sequence = 3;
    fragment.stream = aerosim::LogStream::Stderr;
    AEROSIM_CHECK(context, fragment.isValid());
    AEROSIM_CHECK(context, !fragment.replacesCurrentLine());

    // Carriage-return progress output replaces the current line.
    fragment.updateMode = aerosim::LogUpdateMode::ReplaceCurrentLine;
    AEROSIM_CHECK(context, fragment.replacesCurrentLine());
    fragment.updateMode = aerosim::LogUpdateMode::AppendChunk;
    AEROSIM_CHECK(context, !fragment.replacesCurrentLine());

    // A fragment has no durable resource reference; a record reference does.
    aerosim::LogRecordRef recordRef;
    AEROSIM_CHECK(context, !recordRef.isValid());
    recordRef.resource = verifiedResource(QStringLiteral("log-a"),
                                          aerosim::ResourceType::Log);
    recordRef.task = taskRef();
    recordRef.attempt = attemptRef();
    AEROSIM_CHECK(context, recordRef.isValid());

    // A log resource must be typed as a log.
    recordRef.resource.type = aerosim::ResourceType::Monitor;
    AEROSIM_CHECK(context, !recordRef.isValid());
    recordRef.resource.type = aerosim::ResourceType::Log;

    // The attempt must belong to the referenced task.
    recordRef.attempt.task.taskId = QStringLiteral("task-b");
    AEROSIM_CHECK(context, !recordRef.isValid());
}

void checkMonitorSample(aerosim_test::TestContext &context)
{
    aerosim::MonitorSampleV2 record;
    AEROSIM_CHECK(context, !record.isValid());
    record = sample(QStringLiteral("residual-series"),
                    aerosim::MonitorQuantity::Residual,
                    QStringLiteral("residual_l2"), QString(), 1.0e-6, 100, 1);
    AEROSIM_CHECK(context, record.isValid());
    // A dimensionless residual has no unit; time and step are both carried.
    AEROSIM_CHECK(context, record.unit.isEmpty());
    AEROSIM_CHECK(context, record.step == 100);
    AEROSIM_CHECK(context, !record.hasTime());
    record.time = QDateTime::currentDateTimeUtc();
    AEROSIM_CHECK(context, record.hasTime());

    // The attempt id is mandatory so retries never mix in one curve.
    record.attemptId.clear();
    AEROSIM_CHECK(context, !record.isValid());
    record.attemptId = QStringLiteral("attempt-1");

    record.metricKey.clear();
    AEROSIM_CHECK(context, !record.isValid());
    record.metricKey = QStringLiteral("residual_l2");

    record.seriesId.clear();
    AEROSIM_CHECK(context, !record.isValid());
    record.seriesId = QStringLiteral("residual-series");

    record.value = std::numeric_limits<double>::quiet_NaN();
    AEROSIM_CHECK(context, !record.isValid());
    record.value = 1.0e-6;

    // A sample must belong to the project of its task.
    record.projectId = QStringLiteral("project-b");
    AEROSIM_CHECK(context, !record.isValid());
}

void checkMonitorSeriesAndBatch(aerosim_test::TestContext &context)
{
    aerosim::MonitorSeries series;
    AEROSIM_CHECK(context, !series.isValid());

    series.seriesId = QStringLiteral("lift-series");
    series.quantity = aerosim::MonitorQuantity::Lift;
    series.metricKey = QStringLiteral("cl");
    series.unit = QStringLiteral("1");
    series.timeUnit = QStringLiteral("s");
    series.samples.append(sample(series.seriesId, series.quantity, series.metricKey,
                                series.unit, 0.42, 1, 1));
    series.samples.append(sample(series.seriesId, series.quantity, series.metricKey,
                                series.unit, 0.44, 2, 2));
    AEROSIM_CHECK(context, series.isValid());
    AEROSIM_CHECK(context, series.samples.size() == 2);
    AEROSIM_CHECK(context, series.hasUniformUnit());

    // A sample from another series cannot be mixed into this one.
    aerosim::MonitorSeries mixed = series;
    mixed.samples.append(sample(QStringLiteral("drag-series"),
                               aerosim::MonitorQuantity::Drag,
                               QStringLiteral("cd"), QStringLiteral("1"),
                               0.11, 3, 3));
    AEROSIM_CHECK(context, !mixed.isValid());

    aerosim::MonitorSeries drifted = series;
    drifted.unit = QStringLiteral("N");
    AEROSIM_CHECK(context, !drifted.hasUniformUnit());

    // Residual, lift, drag and moment series carry explicit units.
    const aerosim::MonitorQuantity quantities[] = {
        aerosim::MonitorQuantity::Residual,
        aerosim::MonitorQuantity::Lift,
        aerosim::MonitorQuantity::Drag,
        aerosim::MonitorQuantity::Moment
    };
    for (aerosim::MonitorQuantity quantity : quantities) {
        AEROSIM_CHECK(context,
            !aerosim::enumToken(quantity).isEmpty());
    }

    aerosim::MonitorBatch batch;
    AEROSIM_CHECK(context, !batch.isValid());
    batch.samples = series.samples;
    AEROSIM_CHECK(context, batch.isValid());

    aerosim::MonitorSeriesRef seriesRef;
    AEROSIM_CHECK(context, !seriesRef.isValid());
    seriesRef.resource = verifiedResource(QStringLiteral("monitor-a"),
                                          aerosim::ResourceType::Monitor);
    seriesRef.task = taskRef();
    seriesRef.attempt = attemptRef();
    seriesRef.seriesId = QStringLiteral("lift-series");
    AEROSIM_CHECK(context, seriesRef.isValid());

    seriesRef.resource.type = aerosim::ResourceType::Log;
    AEROSIM_CHECK(context, !seriesRef.isValid());
    seriesRef.resource.type = aerosim::ResourceType::Monitor;
    seriesRef.seriesId.clear();
    AEROSIM_CHECK(context, !seriesRef.isValid());
}

void checkTokens(aerosim_test::TestContext &context)
{
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::LogStream::Stderr) == QStringLiteral("stderr"));
    AEROSIM_CHECK(context,
        aerosim::logStreamFromToken(QStringLiteral("stdout")).value()
            == aerosim::LogStream::Stdout);
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::LogUpdateMode::ReplaceCurrentLine)
            == QStringLiteral("replace_current_line"));
    AEROSIM_CHECK(context,
        aerosim::logUpdateModeFromToken(QStringLiteral("append_chunk")).value()
            == aerosim::LogUpdateMode::AppendChunk);
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::MonitorQuantity::Moment)
            == QStringLiteral("moment"));
    AEROSIM_CHECK(context,
        aerosim::monitorQuantityFromToken(QStringLiteral("residual")).value()
            == aerosim::MonitorQuantity::Residual);
    AEROSIM_CHECK(context,
        aerosim::monitorQuantityFromToken(QStringLiteral("nope")).status()
            == aerosim::AccessStatus::InvalidArgument);
}

} // namespace

int main()
{
    aerosim_test::TestContext context;
    checkLogEntry(context);
    checkLogFragmentAndModes(context);
    checkMonitorSample(context);
    checkMonitorSeriesAndBatch(context);
    checkTokens(context);
    return context.result();
}
