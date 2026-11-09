#pragma once

#include "aerosim/contracts/References.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace aerosim {

enum class LogLevel { Debug, Info, Warning, Error };

// Legacy aggregate log record kept for existing callers. New code uses the
// attempt-aware LogEntryV2 below.
struct LogEntry {
    QDateTime timestamp;
    LogLevel level = LogLevel::Info;
    QString source;
    QString taskId;
    QString message;
};

struct ErrorInfo {
    QString code;
    QString source;
    QString taskId;
    QString message;
    QString detail;
};

struct MonitorSample {
    QString taskId;
    QDateTime timestamp;
    quint64 sequence = 0;
    QString metric;
    double value = 0.0;
    QString unit;
};

// Where a line came from: the application itself or the solver process.
enum class LogStream { Application, Stdout, Stderr };

// How a fragment updates the view. AppendChunk appends without a guaranteed line
// break, ReplaceCurrentLine implements carriage-return progress output.
enum class LogUpdateMode { AppendLine, AppendChunk, ReplaceCurrentLine };

// Reference to the durable log record of one attempt. The runtime owns the file;
// contracts only carry identity and provenance.
struct LogRecordRef {
    ResourceRefV2 resource;      // must be typed ResourceType::Log
    TaskRef task;
    AttemptRef attempt;

    bool isValid() const;
};

// Persisted, sequence-ordered log record. This is the durable unit of output.
struct LogEntryV2 {
    QString projectId;
    TaskRef task;
    QString attemptId;
    quint64 sequence = 0;
    QDateTime timestamp;
    LogLevel level = LogLevel::Info;
    LogStream stream = LogStream::Application;
    LogUpdateMode updateMode = LogUpdateMode::AppendLine;
    QString source;
    QString text;

    bool isValid() const;
};

// Ephemeral display fragment. logging_ui/UI own it, it is never persisted and it
// must not be treated as a task fact.
struct LogFragment {
    TaskRef task;
    QString attemptId;
    quint64 sequence = 0;
    LogLevel level = LogLevel::Info;
    LogStream stream = LogStream::Application;
    LogUpdateMode updateMode = LogUpdateMode::AppendLine;
    QString text;

    bool isValid() const;
    bool replacesCurrentLine() const;
};

// Monitored physical quantities. metricKey stays free-form so unmodelled
// quantities remain representable.
enum class MonitorQuantity { Residual, Lift, Drag, Moment, Coefficient, Other };

struct MonitorSeriesRef {
    ResourceRefV2 resource;      // must be typed ResourceType::Monitor
    TaskRef task;
    AttemptRef attempt;
    QString seriesId;

    bool isValid() const;
};

// One sample of a monitored series. Values are always associated with an attempt
// so curves from different retries never mix.
struct MonitorSampleV2 {
    QString projectId;
    TaskRef task;
    QString attemptId;
    QString seriesId;
    MonitorQuantity quantity = MonitorQuantity::Other;
    QString metricKey;
    QString component;
    QString unit;
    quint64 step = 0;
    QDateTime time;
    double value = 0.0;
    quint64 sequence = 0;

    bool isValid() const;
    bool hasTime() const;
};

struct MonitorSeries {
    QString seriesId;
    MonitorQuantity quantity = MonitorQuantity::Other;
    QString metricKey;
    QString component;
    QString unit;
    QString timeUnit;
    QVector<MonitorSampleV2> samples;

    bool isValid() const;
    bool hasUniformUnit() const;
};

// Batch delivery keeps high-frequency monitoring off the per-sample signal path.
struct MonitorBatch {
    QVector<MonitorSampleV2> samples;

    bool isValid() const;
};

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::LogEntry)
Q_DECLARE_METATYPE(aerosim::ErrorInfo)
Q_DECLARE_METATYPE(aerosim::MonitorSample)
Q_DECLARE_METATYPE(aerosim::LogRecordRef)
Q_DECLARE_METATYPE(aerosim::LogEntryV2)
Q_DECLARE_METATYPE(aerosim::LogFragment)
Q_DECLARE_METATYPE(aerosim::MonitorSeriesRef)
Q_DECLARE_METATYPE(aerosim::MonitorSampleV2)
Q_DECLARE_METATYPE(aerosim::MonitorSeries)
Q_DECLARE_METATYPE(aerosim::MonitorBatch)

