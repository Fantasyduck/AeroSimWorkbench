#pragma once

#include "MonitorLiveSource.h"

#include <QHash>
#include <QString>
#include <QVector>

namespace aerosim {
namespace monitor {

// Grows one MonitorLog from a sequence of live updates.
//
// The chart and the series model both speak MonitorLog, so the live path ends
// where the offline path ends: every delivery is folded into a single log and
// the rest of the stack never learns that the data arrived in pieces. Appending
// here also keeps the awkward cases (a column appearing mid-run, a solver that
// restarts, an axis that has to be continued rather than overwritten) in one
// place instead of in every caller.
class MonitorLogAccumulator
{
public:
    // Identity of the accumulated data. Curve ids are built from the dataset id,
    // so leaving it empty would make every curve arriving from a live source
    // indistinguishable from the same column in another run.
    void setDatasetId(const QString &datasetId);
    QString datasetId() const { return log_.datasetId; }

    void reset();

    // Folds one delivery into the accumulated log. Returns false only when the
    // delivery itself is malformed; a restart is applied rather than rejected.
    bool append(const MonitorUpdate &update, QString *error);

    const MonitorLog &log() const { return log_; }
    // Bumped on every accepted append, so a view can skip redrawing when the
    // source produced nothing new.
    qint64 revision() const { return revision_; }
    int rowCount() const { return log_.iterationStep.size(); }

private:
    // Makes room for columns this update introduced after the first delivery.
    void widen(const QVector<MonitorColumn> &incoming);
    // Appends one value per known column, filling gaps with NaN so a column that
    // stops being reported leaves a hole instead of shifting every later row.
    void appendRowValues(const QVector<QVector<double>> &values,
                         const QHash<int, int> &incomingIndex);
    // How many rows of one index column differ from its first value. Which index
    // becomes the iteration axis depends on this, and it keeps changing while a
    // solver runs, so it is brought up to date on every delivery rather than
    // frozen at the first one. Only the rows that arrived since the previous
    // delivery are examined: rescanning the whole column per delivery made a long
    // run quadratic, and a run that never ends made it unusable.
    qint64 progressionOf(const QString &columnKey) const;

    MonitorLog log_;
    // Kept across reset(): a restart of the same solver is still the same
    // dataset, and the curves on screen must keep their identity.
    QString datasetId_;
    qint64 revision_ = 1;
    // Position each accumulated column has in an incoming update; rebuilt per
    // delivery because a source may reorder or narrow what it sends.
    QHash<int, int> incomingIndex_;
    // Running progression per index column, and how many of its rows have already
    // been folded into that count. mutable because progressionOf() is a query that
    // keeps its own bookkeeping; nothing observable changes when it runs.
    mutable QHash<QString, qint64> progressionCount_;
    mutable QHash<QString, int> progressionScanned_;
};

} // namespace monitor
} // namespace aerosim
