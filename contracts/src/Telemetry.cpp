#include "aerosim/contracts/Telemetry.h"

namespace aerosim {

bool LogRecordRef::isValid() const
{
    if (!resource.isValid() || resource.type != ResourceType::Log)
        return false;
    if (!task.isValid() || !attempt.isValid())
        return false;
    return attempt.task.sameIdentity(task);
}

bool LogEntryV2::isValid() const
{
    if (!task.isValid() || attemptId.isEmpty() || sequence == 0)
        return false;
    if (!projectId.isEmpty() && projectId != task.projectId)
        return false;
    return true;
}

bool LogFragment::isValid() const
{
    return !attemptId.isEmpty() && sequence > 0;
}

bool LogFragment::replacesCurrentLine() const
{
    return updateMode == LogUpdateMode::ReplaceCurrentLine;
}

bool MonitorSeriesRef::isValid() const
{
    if (!resource.isValid() || resource.type != ResourceType::Monitor)
        return false;
    if (!task.isValid() || !attempt.isValid())
        return false;
    if (!attempt.task.sameIdentity(task))
        return false;
    return !seriesId.isEmpty();
}

bool MonitorSampleV2::isValid() const
{
    if (!task.isValid() || attemptId.isEmpty())
        return false;
    if (seriesId.isEmpty() || metricKey.isEmpty())
        return false;
    if (!projectId.isEmpty() && projectId != task.projectId)
        return false;
    return qIsFinite(value);
}

bool MonitorSampleV2::hasTime() const
{
    return time.isValid();
}

bool MonitorSeries::isValid() const
{
    if (seriesId.isEmpty() || metricKey.isEmpty() || samples.isEmpty())
        return false;
    for (const MonitorSampleV2 &sample : samples) {
        if (!sample.isValid() || sample.seriesId != seriesId)
            return false;
    }
    return true;
}

bool MonitorSeries::hasUniformUnit() const
{
    for (const MonitorSampleV2 &sample : samples) {
        if (sample.unit != unit)
            return false;
    }
    return true;
}

bool MonitorBatch::isValid() const
{
    if (samples.isEmpty())
        return false;
    for (const MonitorSampleV2 &sample : samples) {
        if (!sample.isValid())
            return false;
    }
    return true;
}

} // namespace aerosim
