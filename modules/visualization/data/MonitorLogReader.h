#pragma once

#include <QHash>
#include <functional>
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
// A naming convention for solver history headers.
//
// The built-in rules below are SU2-shaped (rms[...], CD, Inner_Iter). They are
// all we have evidence for, and guessing more would encode assumptions about
// files nobody has seen yet. So a second convention is *registered* instead of
// being hard-coded: the reader consults the built-in rules first and the
// registered ones only for names the built-ins do not claim. With nothing
// registered the behaviour is exactly what it was before this existed.
//
// Any member may be left empty; an empty member simply contributes nothing.
struct MonitorColumnRules {
    QString id;                                       // stable name, e.g. "openfoam"
    QString description;                              // shown where a UI lists them
    std::function<bool(const QString &)> isResidual;  // suggests a logarithmic axis
    std::function<bool(const QString &)> isAxis;      // index column, not a series
    std::function<QString(const QString &)> unitFor;  // empty string means unknown

    // Which column carries the independent variable. The built-in chain looks
    // only for Outer_Iter / Inner_Iter / Time_Iter, so a convention that names
    // its step column something else (Time, Iteration, step) would otherwise be
    // plotted against a row index and silently lose the real step values.
    std::function<bool(const QString &)> isPrimaryAxis;
};

// Registry of non-built-in column conventions.
//
// Deliberately a process-wide registry rather than a parameter threaded through
// every call: the rules describe a *file format*, and a format does not change
// per call site. It is configured once at start-up. Registration and reading are
// not synchronised against each other, matching the rest of this module -- do
// not register while another thread is reading.
class MonitorColumnRegistry {
public:
    static MonitorColumnRegistry &instance();

    // Appended; earlier registrations are consulted first. Re-registering the
    // same id replaces the previous entry so a caller can reconfigure without
    // accumulating duplicates.
    void registerRules(const MonitorColumnRules &rules);
    void clearRules();
    QStringList registeredIds() const;

    // Falls back to nothing: returns false / an empty unit when no registered
    // rule claims the key. The built-in rules are applied by the reader before
    // this is consulted.
    bool hasResidualRule(const QString &columnKey) const;
    bool hasAxisRule(const QString &columnKey) const;
    bool hasPrimaryAxisRule(const QString &columnKey) const;
    QString unitFromRules(const QString &columnKey) const;

private:
    MonitorColumnRegistry() = default;
    QVector<MonitorColumnRules> rules_;
};

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

    // The same for Tecplot ASCII, the format XCFD writes its history and residual
    // curves in. Separate from readCsv() rather than folded into it because the
    // two formats disagree about almost everything below the header -- whitespace
    // versus comma separated, packed by point versus by variable, several ZONE
    // blocks instead of one table -- while agreeing completely about what makes a
    // column a residual, what its unit is, and which column is the x axis. Those
    // shared decisions live in one place so the two readers cannot drift apart.
    static bool readTecplot(const QByteArray &content, const QString &sourcePath,
                            MonitorLog *out, QString *error);

    // Whether a file's bytes are a Tecplot ASCII history this reader can draw.
    // CanRead consults it, and read() uses the same test to pick a reader, so a
    // file offered by a file dialog is a file that actually loads.
    static bool canReadTecplot(const QByteArray &content);

    static QString datasetIdForPath(const QString &path);
    static bool looksLikeResidual(const QString &columnKey);
    static QString unitForColumn(const QString &columnKey);

    // Column classification as the reader itself applies it. Exposed so a live
    // source can classify a header exactly the way an offline read would, without
    // keeping a second copy of the residual/unit/axis rules that could drift.
    static MonitorColumn classifyColumn(const QString &columnKey);

    // Whether a header is a solver history at all, as opposed to any CSV with
    // numeric columns. One predicate, used by both canRead() and readCsv(), so
    // probing a file and reading it cannot disagree about what they accept.
    static bool hasRecognisedColumns(const QVector<MonitorColumn> &columns);

    // One CSV line split into raw fields, honouring quoted headers. Zero-based
    // and lossless: callers trim the fields they keep.
    static QStringList splitCsvLine(const QString &line);
    // One field with its padding and the quotes around it removed. A header read
    // through two different paths has to yield the same column key, otherwise the
    // live curves and the curves read from disk never share an identity.
    static QString trimmedField(const QString &raw);

    // One CSV data cell as a number: padding and the quotes around the value are
    // removed first, and an empty or unparsable cell is reported rather than
    // turned into a zero. Both readers share it so a malformed field degrades the
    // same way whether it arrived live or was read from a finished file.
    static bool parseNumericField(const QString &raw, double *out);

    // One CSV data row aligned to the declared column count, which is the one
    // decision the two readers used to take differently: the offline path dropped
    // a row with too few fields while the tail padded it with missing values and
    // said nothing, so the same file produced different series depending on how
    // it was read. Returns false when the row cannot be attributed to the columns
    // (fewer fields than the header declares); longer rows are truncated, which is
    // a solver writing a column the header does not mention. ragged() reports
    // whether the field count differed from the header at all.
    static bool admitDataRow(const QString &line, int columnCount,
                             QStringList *aligned, bool *ragged);

    // Same admission rule, but aligning through the header's field map instead of
    // assuming field i is column i. The one-argument version above is kept for
    // callers that hold a header with no nameless field, where the two coincide.
    static bool admitDataRow(const QString &line, int columnCount,
                             const QVector<int> &fieldToColumn,
                             QStringList *aligned, bool *ragged);

    // Header line -> columns, in field order, plus the field -> column map that
    // aligns a data row. fieldToColumn has one entry per header field: the index
    // of the column it fills, or -1 for a field that carries no column (a nameless
    // field). Trailing nameless fields are dropped from the map so a header that
    // ends with a comma does not demand that every data row have one too.
    //
    // A duplicated name is renamed rather than dropped, because dropping it shifts
    // every column to its right: the data under one header used to be attributed
    // to its neighbour, which is silent corruption rather than a rejected file.
    //
    // Exposed so the live tail can build its columns with this same code. The two
    // paths used to disagree -- the offline reader de-duplicated and the tail did
    // not -- so the same file produced different columns depending on how it was
    // read.
    static QVector<MonitorColumn> parseHeaderColumns(const QString &headerLine,
                                                     QStringList *warnings,
                                                     QVector<int> *fieldToColumn);

private:
    static bool looksLikeAxis(const QString &columnKey);
};

} // namespace monitor
} // namespace aerosim
