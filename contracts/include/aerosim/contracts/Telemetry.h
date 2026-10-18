#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace aerosim {

enum class LogLevel { Debug, Info, Warning, Error };

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

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::LogEntry)
Q_DECLARE_METATYPE(aerosim::ErrorInfo)
Q_DECLARE_METATYPE(aerosim::MonitorSample)
