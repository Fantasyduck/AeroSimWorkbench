#include "MonitorLogAccumulator.h"

#include <cmath>
#include <limits>

namespace aerosim {
namespace monitor {
namespace {

double missingValue()
{
    return std::numeric_limits<double>::quiet_NaN();
}

} // namespace

void MonitorLogAccumulator::setDatasetId(const QString &datasetId)
{
    datasetId_ = datasetId;
    log_.datasetId = datasetId_;
}

void MonitorLogAccumulator::reset()
{
    log_ = MonitorLog();
    // A restart rewinds the rows, not the identity: the same solver restarting is
    // the same dataset, and the caller keeps appending to the same curves.
    log_.datasetId = datasetId_;
    incomingIndex_.clear();
    progressionCount_.clear();
    progressionScanned_.clear();
    ++revision_;
}

void MonitorLogAccumulator::widen(const QVector<MonitorColumn> &incoming)
{
    const int before = log_.columns.size();
    const int rows = log_.iterationStep.size();
    for (const MonitorColumn &column : incoming) {
        bool known = false;
        for (const MonitorColumn &existing : log_.columns) {
            if (existing.key == column.key) {
                known = true;
                break;
            }
        }
        if (known)
            continue;
        // A late column starts where it first appeared; the rows it missed are
        // holes, not zeros. Writing zeros would draw a flat line through history
        // the solver never reported.
        log_.columns.append(column);
        QVector<double> backfill;
        backfill.reserve(rows);
        for (int row = 0; row < rows; ++row)
            backfill.append(missingValue());
        log_.values.insert(column.key, backfill);
    }
    if (log_.columns.size() != before)
        ++revision_;
}

void MonitorLogAccumulator::appendRowValues(const QVector<QVector<double>> &values,
                                            const QHash<int, int> &incomingIndex)
{
    for (int column = 0; column < log_.columns.size(); ++column) {
        const int source = incomingIndex.value(column, -1);
        QVector<double> &target = log_.values[log_.columns.at(column).key];
        if (source >= 0 && source < values.size())
            target.append(values.at(source));
        else
            target.append(missingValue());
    }
}

bool MonitorLogAccumulator::append(const MonitorUpdate &update, QString *error)
{
    if (error)
        error->clear();

    // A restart arrives together with the first rows of the new run, so it drops
    // everything accumulated so far and then falls through to the normal append:
    // returning here would lose the rows that came with the marker.
    if (update.restarted)
        reset();

    // An empty delivery is the ordinary "nothing new yet" case, not a failure.
    if (update.isEmpty())
        return true;

    const int rows = update.rowCount();
    for (const QVector<double> &column : update.values) {
        if (column.size() != rows) {
            if (error)
                *error = QStringLiteral("实时更新的一列行数与迭代步不一致，已忽略本次更新。");
            return false;
        }
    }

    widen(update.columns);

    // Map accumulated column -> position in this delivery. Rebuilt every time
    // because a source is free to reorder or narrow what it sends.
    incomingIndex_.clear();
    for (int source = 0; source < update.columns.size(); ++source) {
        const QString key = update.columns.at(source).key;
        for (int column = 0; column < log_.columns.size(); ++column) {
            if (log_.columns.at(column).key == key) {
                incomingIndex_.insert(column, source);
                break;
            }
        }
    }

    const int startRow = log_.iterationStep.size();
    log_.iterationStep.reserve(startRow + rows);
    for (int row = 0; row < rows; ++row) {
        qint64 step = update.iterationStep.at(row);
        if (update.axisFromRowIndex)
            step += startRow;                 // continue the fabricated index
        log_.iterationStep.append(step);
    }

    if (!update.timeIndex.isEmpty()) {
        // A stream without a time index leaves the whole column absent, which is
        // what axisAvailable(TimeIndex) already reports as "not in this file".
        if (log_.timeIndex.size() < startRow) {
            while (log_.timeIndex.size() < startRow)
                log_.timeIndex.append(0);
        }
        for (int row = 0; row < rows; ++row)
            log_.timeIndex.append(update.timeIndex.value(row));
    }

    appendRowValues(update.values, incomingIndex_);

    for (const QString &warning : update.warnings) {
        if (!log_.warnings.contains(warning))
            log_.warnings.append(warning);
    }

    // Progression decides which column becomes the iteration axis, so it has to
    // be recomputed as rows arrive rather than frozen at the first delivery.
    log_.outerIterationProgression = 0;
    log_.innerIterationProgression = 0;
    for (const MonitorColumn &column : log_.columns) {
        if (column.key.compare(QLatin1String("Outer_Iter"), Qt::CaseInsensitive) == 0)
            log_.outerIterationProgression = progressionOf(column.key);
        else if (column.key.compare(QLatin1String("Inner_Iter"), Qt::CaseInsensitive) == 0)
            log_.innerIterationProgression = progressionOf(column.key);
    }

    ++revision_;
    return true;
}

qint64 MonitorLogAccumulator::progressionOf(const QString &columnKey) const
{
    const auto it = log_.values.constFind(columnKey);
    if (it == log_.values.constEnd() || it.value().isEmpty())
        return 0;
    const QVector<double> &column = it.value();

    // Rows are never rewritten once appended, so the count for the rows already
    // seen cannot change; only the tail is new work. When a column shrinks (a
    // restart) the memo is stale and the count restarts from scratch.
    int &scanned = progressionScanned_[columnKey];
    qint64 &changes = progressionCount_[columnKey];
    if (scanned < 1 || scanned > column.size()) {
        scanned = 1;      // the first row is the reference, it never counts
        changes = 0;
    }
    for (int row = scanned; row < column.size(); ++row) {
        if (column.at(row) != column.first() && std::isfinite(column.at(row)))
            ++changes;
    }
    scanned = column.size();
    return changes;
}

} // namespace monitor
} // namespace aerosim
