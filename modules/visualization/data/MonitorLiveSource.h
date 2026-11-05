#pragma once

#include "MonitorLogReader.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace aerosim {
namespace monitor {

// One incremental delivery from a live solver monitor.
//
// A finished history file is read as a whole (MonitorLogReader::read), but a
// solver that is still running only ever hands out more rows. Modelling that as
// "another whole MonitorLog" would force every caller to diff two snapshots;
// modelling it as a row delta keeps the append obvious and cheap.
struct MonitorUpdate {
    // Columns this delivery carries, in stream order, with the same unit and
    // residual/axis recognition the offline reader applies -- a live run and a
    // finished file must classify their columns identically.
    //
    // The list may widen midway: a solver that starts writing a new coefficient
    // after the first thousand iterations is a real case, not a corrupt file.
    QVector<MonitorColumn> columns;
    // values[columnIndex] holds this update's rows for columns[columnIndex].
    // Every inner vector has exactly rowCount() entries.
    QVector<QVector<double>> values;
    QVector<qint64> iterationStep;
    QVector<qint64> timeIndex;              // empty when the stream has no time index
    QStringList warnings;
    // The source rewound (solver restarted, file truncated and rewritten).
    // Whatever was accumulated so far describes a run that no longer exists.
    bool restarted = false;
    // The source had no iteration column and fabricated the step from the row
    // index, so these numbers restart at zero in every delivery. The appender
    // continues them from the row count instead of overwriting history.
    bool axisFromRowIndex = false;

    int rowCount() const { return iterationStep.size(); }
    bool isEmpty() const { return iterationStep.isEmpty() && !restarted; }
};

// Source-agnostic port for live solver monitor data.
//
// Deliberately pull-based and free of QObject: a history file that grows, a
// child process' stdout, a socket, or the existing MonitorSample telemetry
// contract can all sit behind this interface, and none of them forces the data
// layer to know about an event loop. Callers drive poll() from whatever timer
// or readyRead() they already have, which also keeps this class testable with
// no application object at all.
class MonitorLiveSource
{
public:
    virtual ~MonitorLiveSource() {}

    // Stable identity for the dataset this source feeds. Callers use it as the
    // curve datasetId, so a live run and an offline file of the same name never
    // collide in one drawing.
    virtual QString sourceId() const = 0;

    virtual bool start(QString *error) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

    // Returns everything that arrived since the previous call. An empty update
    // means nothing new arrived; false means the source cannot continue and
    // *error says why. Callers keep polling after a false return only if they
    // want to retry a transient failure.
    virtual bool poll(MonitorUpdate *update, QString *error) = 0;
};

} // namespace monitor
} // namespace aerosim
