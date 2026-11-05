#include "MonitorFileTailSource.h"

#include "MonitorLogReader.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QtGlobal>

#include <cmath>
#include <limits>

namespace aerosim {
namespace monitor {
namespace {

double missingValue()
{
    return std::numeric_limits<double>::quiet_NaN();
}

// Mirrors the offline reader's number handling, so a live row and a row read
// from a finished file degrade the same way on a malformed field.
double parseNumber(const QString &text)
{
    bool converted = false;
    const double value = text.trimmed().toDouble(&converted);
    if (!converted || !std::isfinite(value))
        return missingValue();
    return value;
}

} // namespace

MonitorFileTailSource::MonitorFileTailSource(const QString &path)
    : path_(path)
    , sourceId_(MonitorLogReader::datasetIdForPath(path))
{
}

MonitorFileTailSource::~MonitorFileTailSource() = default;

QString MonitorFileTailSource::sourceId() const
{
    return sourceId_;
}

bool MonitorFileTailSource::start(QString *error)
{
    if (error)
        error->clear();
    const QFileInfo info(path_);
    if (!info.exists() || !info.isFile()) {
        if (error)
            *error = QStringLiteral("找不到要跟随的历史文件：%1").arg(path_);
        return false;
    }
    pending_.clear();
    offset_ = 0;
    headerParsed_ = false;
    columns_.clear();
    axisFromRowIndex_ = false;
    reportedRows_ = 0;
    running_ = true;
    return true;
}

void MonitorFileTailSource::stop()
{
    running_ = false;
}

bool MonitorFileTailSource::isRunning() const
{
    return running_;
}

bool MonitorFileTailSource::parseHeader(QString *error)
{
    const int newline = pending_.indexOf('\n');
    if (newline < 0)
        return false;   // the header line itself has not fully arrived yet

    QByteArray line = pending_.left(newline);
    if (line.endsWith('\r'))
        line.chop(1);
    if (line.startsWith(QByteArray("\xEF\xBB\xBF")))
        line.remove(0, 3);

    const QStringList fields = MonitorLogReader::splitCsvLine(QString::fromUtf8(line));
    columns_.clear();
    for (const QString &field : fields) {
        // Same normalisation the offline reader applies, so a column keeps its
        // key (and therefore its curve identity) whichever path read the file.
        const QString key = MonitorLogReader::trimmedField(field);
        if (key.isEmpty())
            continue;
        columns_.append(MonitorLogReader::classifyColumn(key));
    }
    if (columns_.isEmpty()) {
        if (error)
            *error = QStringLiteral("历史文件的表头没有可识别的列。");
        return false;
    }

    bool hasIndex = false;
    for (const MonitorColumn &column : columns_) {
        if (column.isAxis) {
            hasIndex = true;
            break;
        }
    }
    // Without an iteration column the row number is the only honest axis.
    axisFromRowIndex_ = !hasIndex;

    pending_.remove(0, newline + 1);
    headerParsed_ = true;
    return true;
}

void MonitorFileTailSource::parseRows(const QStringList &lines, MonitorUpdate *update) const
{
    for (const QString &line : lines) {
        const QStringList fields = MonitorLogReader::splitCsvLine(line);
        if (fields.isEmpty())
            continue;

        // One value slot per declared column, in header order: the accumulator
        // maps them by key, so this class never needs a schema beyond the header
        // it just read.
        for (int column = 0; column < columns_.size(); ++column) {
            if (update->values.size() <= column)
                update->values.append(QVector<double>());
            update->values[column].append(column < fields.size()
                                              ? parseNumber(MonitorLogReader::trimmedField(fields.at(column)))
                                              : missingValue());
        }
        update->iterationStep.append(0);   // filled in by poll()
    }
}

bool MonitorFileTailSource::poll(MonitorUpdate *update, QString *error)
{
    if (error)
        error->clear();
    if (!update)
        return false;
    *update = MonitorUpdate();
    if (!running_)
        return true;

    const QFileInfo info(path_);
    if (!info.exists()) {
        if (error)
            *error = QStringLiteral("跟随的历史文件已不存在：%1").arg(path_);
        return false;
    }

    const qint64 size = info.size();
    // A shrinking file is the solver rewriting its history for a fresh run: the
    // reader rewinds and reports the marker, and the accumulator starts over.
    if (size < offset_) {
        offset_ = 0;
        pending_.clear();
        headerParsed_ = false;
        reportedRows_ = 0;
        update->restarted = true;
        update->warnings.append(QStringLiteral("历史文件被重写，已按新内容重新读取。"));
    }

    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("无法读取历史文件：%1").arg(path_);
        return false;
    }
    if (!file.seek(offset_)) {
        file.close();
        if (error)
            *error = QStringLiteral("无法定位到历史文件的读取位置。");
        return false;
    }
    // Only the bytes appended since the previous poll are read; that is what
    // makes this a tail rather than a re-read.
    const QByteArray chunk = file.readAll();
    file.close();
    if (chunk.isEmpty())
        return true;

    offset_ += chunk.size();
    pending_.append(chunk);

    if (!headerParsed_ && !parseHeader(error)) {
        if (error && !error->isEmpty())
            return false;
        return true;   // header still incomplete
    }

    // Whole lines only. The trailing partial line stays in pending_, so a row
    // that is still being written is never parsed half-way.
    const QList<QByteArray> raw = pending_.split('\n');
    pending_ = raw.isEmpty() ? QByteArray() : raw.last();
    QStringList lines;
    for (int i = 0; i < raw.size() - 1; ++i) {
        QByteArray line = raw.at(i);
        if (line.endsWith('\r'))
            line.chop(1);
        if (!line.trimmed().isEmpty())
            lines.append(QString::fromUtf8(line));
    }

    update->columns = columns_;
    update->axisFromRowIndex = axisFromRowIndex_;
    parseRows(lines, update);

    if (axisFromRowIndex_) {
        // Continue the fabricated index across polls instead of restarting at
        // zero each time. The accumulator rebases it onto the rows it already has.
        for (int row = 0; row < update->iterationStep.size(); ++row)
            update->iterationStep[row] = reportedRows_ + row;
    } else {
        const int index = indexColumn();
        if (index >= 0 && index < update->values.size()) {
            const QVector<double> &column = update->values.at(index);
            for (int row = 0; row < update->iterationStep.size(); ++row) {
                const double value = row < column.size() ? column.at(row) : missingValue();
                update->iterationStep[row] = std::isfinite(value)
                                                 ? static_cast<qint64>(value)
                                                 : reportedRows_ + row;
            }
        }
    }
    reportedRows_ += update->iterationStep.size();
    return true;
}

int MonitorFileTailSource::indexColumn() const
{
    // Prefer the outer iteration, else the inner one, so the axis matches what
    // the offline reader would have picked for the same file.
    int inner = -1;
    for (int i = 0; i < columns_.size(); ++i) {
        const QString &key = columns_.at(i).key;
        if (key.compare(QLatin1String("Outer_Iter"), Qt::CaseInsensitive) == 0)
            return i;
        if (key.compare(QLatin1String("Inner_Iter"), Qt::CaseInsensitive) == 0)
            inner = i;
    }
    return inner;
}

} // namespace monitor
} // namespace aerosim
