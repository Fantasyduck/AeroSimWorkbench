#include "aerosim/contracts/Telemetry.h"

#include <type_traits>

// The legacy LogEntry/MonitorSample aggregates were removed because no module
// consumed them; these V2 types are the single telemetry model now.
static_assert(std::is_copy_constructible<aerosim::LogEntryV2>::value,
    "LogEntryV2 must remain a copyable value type");
static_assert(std::is_copy_constructible<aerosim::LogFragment>::value,
    "LogFragment must remain a copyable value type");
static_assert(std::is_copy_constructible<aerosim::MonitorSampleV2>::value,
    "MonitorSampleV2 must remain a copyable value type");
static_assert(std::is_copy_constructible<aerosim::MonitorBatch>::value,
    "MonitorBatch must remain a copyable value type");
static_assert(std::is_copy_constructible<aerosim::ErrorInfo>::value,
    "ErrorInfo must remain a copyable value type");

int compileTelemetryHeader()
{
    aerosim::LogEntryV2 entry;
    entry.sequence = 1;
    aerosim::MonitorBatch batch;
    batch.samples.append(aerosim::MonitorSampleV2());
    return entry.isValid() ? 1 : 0;
}
