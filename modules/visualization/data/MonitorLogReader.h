#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace aerosim {
namespace monitor {

// One numeric column of a solver history file. The reader never assumes a fixed
// column set: names come from the file header, so solver configurations that add
// or drop columns (for example SU2's optional "RefForce") stay readable.
struct MonitorColumn {
    QString key;                        // canonical name: trimmed, unquoted
    QString unit;                       // inferred unit, empty when unknown
    bool isResidual = false;            // residual column, suggested logarithmic
    bool isAxis = false;                // iteration/time index, not a plottable series
};

// Offline history log read from a solver monitor file. Deliberately separate
// from the online Telemetry::MonitorSample contract: that one streams a single
// metric from a live task, this one is a finished, bulk, multi-column file.
struct MonitorLog {
    QString datasetId;
    QString sourcePath;
    QVector<MonitorColumn> columns;
    QVector<qint64> iterationStep;              // Outer_Iter, else Inner_Iter, else row index
    QVector<qint64> timeIndex;                  // Time_Iter, empty when absent
    QHash<QString, QVector<double>> values;     // column key -> column data
    QStringList warnings;                       // non-fatal problems, never silent
    qint64 outerIterationProgression = 0;       // rows behind the iteration axis when Outer_Iter advances
    qint64 innerIterationProgression = 0;       // rows behind the iteration axis when Inner_Iter advances
};

// Reads solver history CSV files (SU2 style) with no Qt Widget and no VTK
// dependency, so the data path stays usable in headless builds.
class MonitorLogReader {
public:
    static bool canRead(const QString &path);
    static bool read(const QString &path, MonitorLog *out, QString *error);

    // Parses an in-memory file; separated from read() so tests need no temp files.
    static bool readCsv(const QByteArray &content, const QString &sourcePath,
                        MonitorLog *out, QString *error);

    static QString datasetIdForPath(const QString &path);
    static bool looksLikeResidual(const QString &columnKey);
    static QString unitForColumn(const QString &columnKey);

    // Column classification as the reader itself applies it. Exposed so a live
    // source can classify a header exactly the way an offline read would, without
    // keeping a second copy of the residual/unit/axis rules that could drift.
    static MonitorColumn classifyColumn(const QString &columnKey);

    // One CSV line split into raw fields, honouring quoted headers. Zero-based
    // and lossless: callers trim the fields they keep.
    static QStringList splitCsvLine(const QString &line);
    // One field with its padding and the quotes around it removed. A header read
    // through two different paths has to yield the same column key, otherwise the
    // live curves and the curves read from disk never share an identity.
    static QString trimmedField(const QString &raw);

private:
    static bool looksLikeAxis(const QString &columnKey);
};

} // namespace monitor
} // namespace aerosim
