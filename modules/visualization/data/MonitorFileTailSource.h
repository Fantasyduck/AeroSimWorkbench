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

    // Rows this run dropped because they could not be attributed to the columns,
    // and rows whose field count differed from the header. The warning a delivery
    // carries says that it happened; these say how often, which is what a caller
    // logging a live run needs.
    qint64 skippedRows() const;
    qint64 raggedRows() const;
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
    void parseRows(const QStringList &lines, MonitorUpdate *update,
                    int *skipped, int *ragged);
    // Decides which column becomes the iteration axis, the way the offline reader
    // does: whichever of Outer_Iter and Inner_Iter actually advances wins, then a
    // column a registered rule calls the primary axis, then the row number. A tail
    // cannot look at the whole file, so the decision is taken from the deliveries
    // it has already seen and then held fixed.
    void resolveAxis(MonitorUpdate *update);

    // Header position of a built-in index column, or -1.
    int builtInIndex(const QString &name) const;

    QString path_;
    QString sourceId_;
    QByteArray pending_;        // partial trailing line, kept between polls
    qint64 offset_ = 0;         // bytes already consumed
    bool running_ = false;
    QVector<MonitorColumn> columns_;
    // Header field -> column index, so a row is aligned by position. Held beside
    // the columns because a header may name a field nobody fills (a trailing
    // comma) and may repeat a name that had to be renamed.
    QVector<int> fieldToColumn_;
    // Warnings the shared header parser produced (a renamed duplicate column).
    // Held until the first delivery instead of being dropped, because a renamed
    // column changes the curve identity the caller sees.
    QStringList headerWarnings_;
    bool headerParsed_ = false;
    // Fabricate the iteration step from the row index: for a file that has no
    // iteration column at all.
    bool axisFromRowIndex_ = false;
    qint64 reportedRows_ = 0;   // rows handed out so far, used for that fabrication
    // Header position of the column a registered rule calls the primary axis, and
    // the axis column once it has been resolved (-1 until the first rows arrive).
    int primaryAxisIndex_ = -1;
    int axisColumn_ = -1;
    bool axisResolved_ = false;
    // Running progression tally per built-in index column, so choosing between
    // Outer_Iter and Inner_Iter does not re-scan every row already delivered.
    QHash<int, int> axisChangeCount_;
    QHash<int, double> axisLastValue_;
    // Sticky: a file that is not plain UTF-8 does not repair itself, so reporting
    // the problem once and then letting later polls look healthy would be a lie.
    QString fatalError_;
    // Running totals of rows the shared admission rule rejected, or accepted with
    // a different field count than the header declares.
    qint64 skippedRows_ = 0;
    qint64 raggedRows_ = 0;
};

} // namespace monitor
} // namespace aerosim
