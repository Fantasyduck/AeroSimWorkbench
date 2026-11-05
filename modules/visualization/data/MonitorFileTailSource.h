#pragma once

#include "MonitorLiveSource.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace aerosim {
namespace monitor {

// Live source that follows a solver history file as the solver appends to it.
//
// This is the reference implementation of MonitorLiveSource, not the only
// intended one: a solver that reports over stdout or a socket implements the
// same port and reuses MonitorLogAccumulator unchanged. It reads only the bytes
// appended since the previous poll and keeps a partial trailing line for next
// time, so a row is never parsed half-written.
class MonitorFileTailSource : public MonitorLiveSource
{
public:
    explicit MonitorFileTailSource(const QString &path);
    ~MonitorFileTailSource() override;

    QString sourceId() const override;
    bool start(QString *error) override;
    void stop() override;
    bool isRunning() const override;
    bool poll(MonitorUpdate *update, QString *error) override;

    // Offset already consumed, exposed so a test can assert that a poll really
    // moved the read position instead of re-parsing the whole file.
    qint64 consumedBytes() const { return offset_; }

private:
    // Parses the header line into columns, remembering how many index columns the
    // file declares so row parsing does not repeat the recognition.
    bool parseHeader(QString *error);
    // Turns a batch of complete data lines into one delivery. The iteration step
    // is left at zero: only poll() knows which column carries it.
    void parseRows(const QStringList &lines, MonitorUpdate *update) const;
    // Header position of the column that should become the iteration axis, or -1.
    int indexColumn() const;

    QString path_;
    QString sourceId_;
    QByteArray pending_;        // partial trailing line, kept between polls
    qint64 offset_ = 0;         // bytes already consumed
    bool running_ = false;
    QVector<MonitorColumn> columns_;
    bool headerParsed_ = false;
    // Fabricate the iteration step from the row index: for a file that has no
    // iteration column at all.
    bool axisFromRowIndex_ = false;
    qint64 reportedRows_ = 0;   // rows handed out so far, used for that fabrication
};

} // namespace monitor
} // namespace aerosim
