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

// Strict UTF-8 validation over bytes, applied to whole lines only.
//
// QString::fromUtf8 is lenient: it substitutes U+FFFD for a malformed sequence
// and carries on, which would turn a mis-encoded solver column into a curve of
// plausible-looking numbers. The offline reader catches this by scanning the
// decoded text for U+FFFD; a tail cannot decode first, because a multi-byte
// character may be split across two polls. Validating the bytes of a complete
// line sidesteps both problems and yields the same verdict as the offline path.
bool isUtf8(const QByteArray &bytes)
{
    const int size = bytes.size();
    int index = 0;
    while (index < size) {
        const unsigned char lead = static_cast<unsigned char>(bytes.at(index));
        int trailing = 0;
        unsigned int codePoint = 0;
        if (lead < 0x80) {
            ++index;
            continue;
        } else if ((lead & 0xE0) == 0xC0) {
            trailing = 1;
            codePoint = lead & 0x1Fu;
        } else if ((lead & 0xF0) == 0xE0) {
            trailing = 2;
            codePoint = lead & 0x0Fu;
        } else if ((lead & 0xF8) == 0xF0) {
            trailing = 3;
            codePoint = lead & 0x07u;
        } else {
            return false;
        }
        if (index + trailing >= size)
            return false;   // truncated sequence: the line ended in the middle
        for (int step = 1; step <= trailing; ++step) {
            const unsigned char next = static_cast<unsigned char>(bytes.at(index + step));
            if ((next & 0xC0) != 0x80)
                return false;
            codePoint = (codePoint << 6) | (next & 0x3Fu);
        }
        // Reject overlong encodings, surrogates and out-of-range values: these
        // decode to U+FFFD and would slip past a lenient decoder unnoticed.
        if ((trailing == 1 && codePoint < 0x80) || (trailing == 2 && codePoint < 0x800)
            || (trailing == 3 && codePoint < 0x10000) || codePoint > 0x10FFFF
            || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
            return false;
        index += trailing + 1;
    }
    return true;
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
    fieldToColumn_.clear();
    headerWarnings_.clear();
    axisFromRowIndex_ = false;
    reportedRows_ = 0;
    primaryAxisIndex_ = -1;
    axisColumn_ = -1;
    axisResolved_ = false;
    axisChangeCount_.clear();
    axisLastValue_.clear();
    fatalError_.clear();
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

qint64 MonitorFileTailSource::skippedRows() const
{
    return skippedRows_;
}

qint64 MonitorFileTailSource::raggedRows() const
{
    return raggedRows_;
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

    // The header names the columns; if its encoding is wrong the keys are wrong too,
    // and every curve built from them is mislabelled rather than merely misplaced.
    if (!isUtf8(line)) {
        fatalError_ = QStringLiteral("文件不是有效的 UTF-8 文本，请先转换为 UTF-8 后再导入。");
        if (error)
            *error = fatalError_;
        return false;
    }

    // Parsed by the offline reader's own code rather than a copy of it: the two
    // paths used to disagree about a repeated header name (the offline reader
    // renamed it, the tail kept both and let one column's data overwrite the
    // other's), and a copy is free to drift again.
    columns_ = MonitorLogReader::parseHeaderColumns(QString::fromUtf8(line), &headerWarnings_,
                                                    &fieldToColumn_);
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

    // A column a registered rule calls the primary axis, found here so the offline
    // reader's own fallback (built-in names first, then the registered convention)
    // is reproduced rather than approximated.
    primaryAxisIndex_ = -1;
    {
        const MonitorColumnRegistry &registry = MonitorColumnRegistry::instance();
        for (int index = 0; index < columns_.size() && primaryAxisIndex_ < 0; ++index) {
            const MonitorColumn &column = columns_.at(index);
            if (column.isAxis && registry.hasPrimaryAxisRule(column.key))
                primaryAxisIndex_ = index;
        }
    }

    pending_.remove(0, newline + 1);
    headerParsed_ = true;
    return true;
}

void MonitorFileTailSource::parseRows(const QStringList &lines, MonitorUpdate *update,
                                            int *skipped, int *ragged)
{
    for (const QString &line : lines) {
        // The same admission rule the offline reader applies, called rather than
        // reimplemented: a row too short to attribute to the columns is dropped
        // here exactly as it is dropped there, instead of being padded into a
        // series of missing values nobody was told about.
        QStringList fields;
        bool raggedRow = false;
        if (!MonitorLogReader::admitDataRow(line, columns_.size(), fieldToColumn_,
                                            &fields, &raggedRow)) {
            ++(*skipped);
            ++(*ragged);
            continue;
        }
        if (raggedRow)
            ++(*ragged);

        // One value slot per declared column, in header order: the accumulator
        // maps them by key, so this class never needs a schema beyond the header
        // it just read.
        for (int column = 0; column < columns_.size(); ++column) {
            if (update->values.size() <= column)
                update->values.append(QVector<double>());
            double value = missingValue();
            MonitorLogReader::parseNumericField(fields.at(column), &value);
            update->values[column].append(value);
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

    // A file that is not plain text does not heal on the next poll. Reporting the
    // problem once and then looking healthy again would silently resume drawing
    // whatever the corrupt bytes happen to parse into.
    if (!fatalError_.isEmpty()) {
        if (error)
            *error = fatalError_;
        return false;
    }

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
        fieldToColumn_.clear();
        headerWarnings_.clear();
        reportedRows_ = 0;
        // The rewritten file is a different run: its columns may differ, so the
        // axis decision and its tallies start over with the rest of the state.
        axisColumn_ = -1;
        axisResolved_ = false;
        primaryAxisIndex_ = -1;
        axisChangeCount_.clear();
        axisLastValue_.clear();
        // The tallies describe this run too, so they rewind with it.
        skippedRows_ = 0;
        raggedRows_ = 0;
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

    // The offline reader refuses these bytes rather than guessing; a tail that
    // accepted them would draw plausible curves from a file nothing else can read.
    // A NUL is never part of valid UTF-8, so this test needs no text decoding and
    // cannot be fooled by a multi-byte character split across two polls.
    if (chunk.contains('\0')) {
        fatalError_ = QStringLiteral("文件包含二进制内容，不是纯文本监控日志。");
        if (error)
            *error = fatalError_;
        return false;
    }

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
        if (line.trimmed().isEmpty())
            continue;
        // Checked per whole line, not per byte: a multi-byte character split across
        // two polls would otherwise look malformed every time a read lands mid-character.
        if (!isUtf8(line)) {
            fatalError_ = QStringLiteral("文件不是有效的 UTF-8 文本，请先转换为 UTF-8 后再导入。");
            if (error)
                *error = fatalError_;
            return false;
        }
        lines.append(QString::fromUtf8(line));
    }

    update->columns = columns_;
    update->axisFromRowIndex = axisFromRowIndex_;
    if (!headerWarnings_.isEmpty()) {
        // Said once, with the first rows: a caller drawing these columns has to
        // know the key it is told about is not the name printed in the header.
        update->warnings.append(headerWarnings_);
        headerWarnings_.clear();
    }
    int skipped = 0;
    int ragged = 0;
    parseRows(lines, update, &skipped, &ragged);
    skippedRows_ += skipped;
    raggedRows_ += ragged;
    // Reported every delivery in which a row was dropped. The text carries no
    // running total on purpose: the accumulator de-duplicates warnings by text,
    // so a total in the message would make a broken file append a new warning
    // per poll for as long as it runs. The counts are available from the source.
    if (skipped > 0)
        update->warnings.append(QStringLiteral("已跳过无法解析的数据行。"));
    if (ragged > 0)
        update->warnings.append(QStringLiteral("有行的字段数与表头不一致，已按表头列数对齐。"));
    resolveAxis(update);

    for (int row = 0; row < update->iterationStep.size(); ++row) {
        if (axisColumn_ < 0 || axisColumn_ >= update->values.size()) {
            // Continue the fabricated index across polls instead of restarting at
            // zero each time. The accumulator rebases it onto the rows it already has.
            update->iterationStep[row] = reportedRows_ + row;
            continue;
        }
        const QVector<double> &column = update->values.at(axisColumn_);
        const double value = row < column.size() ? column.at(row) : missingValue();
        // Rounded, not truncated: the offline reader rounds, and the two paths
        // have to agree on the x coordinate of a row. A step of 1.9 printed by a
        // solver as it accelerates was landing on 1 here and on 2 offline.
        update->iterationStep[row] = std::isfinite(value) ? static_cast<qint64>(std::llround(value))
                                                         : reportedRows_ + row;
    }
    reportedRows_ += update->iterationStep.size();
    return true;
}

void MonitorFileTailSource::resolveAxis(MonitorUpdate *update)
{
    if (axisFromRowIndex_) {
        axisColumn_ = -1;
        return;
    }

    const int outer = builtInIndex(QStringLiteral("Outer_Iter"));
    const int inner = builtInIndex(QStringLiteral("Inner_Iter"));

    // Tally how often each built-in index column has changed value across every
    // delivery so far. Cheap per row (compare with the previous value) and it lets
    // the choice follow the offline rule "whichever one actually advances" without
    // ever re-reading the rows already handed out.
    for (int candidate : {outer, inner}) {
        if (candidate < 0 || candidate >= update->values.size())
            continue;
        const QVector<double> &column = update->values.at(candidate);
        for (double value : column) {
            if (!std::isfinite(value))
                continue;
            const auto last = axisLastValue_.constFind(candidate);
            if (last == axisLastValue_.constEnd()) {
                axisLastValue_.insert(candidate, value);
                continue;
            }
            if (!qFuzzyCompare(last.value() + 1.0, value + 1.0))
                axisChangeCount_[candidate] = axisChangeCount_.value(candidate) + 1;
            axisLastValue_[candidate] = value;
        }
    }

    // Once resolved the choice is held: an axis that has been constant for a hundred
    // rows and moves on row hundred-and-one must not silently renumber the curves.
    if (axisResolved_)
        return;

    int chosen = -1;
    QString warning;
    bool certain = false;
    if (outer >= 0 && axisChangeCount_.value(outer) > 0) {
        chosen = outer;
        certain = true;
    } else if (inner >= 0 && axisChangeCount_.value(inner) > 0) {
        chosen = inner;
        certain = true;
        warning = QStringLiteral("Outer_Iter 未推进，迭代步横轴回退为 Inner_Iter。");
    } else if (primaryAxisIndex_ >= 0) {
        // A registered convention's own time column, used only when the built-in
        // iteration columns are absent or still, exactly as the offline chain does.
        chosen = primaryAxisIndex_;
        certain = true;
        warning = QStringLiteral("迭代步横轴取自注册规则识别的列 %1。")
                      .arg(columns_.at(primaryAxisIndex_).key);
    } else if (outer >= 0) {
        // Nothing has moved yet, so the offline rule has nothing to go on either.
        // Outer_Iter is the conventional choice; stay open to revising it if the
        // next delivery shows the inner loop is the one advancing.
        chosen = outer;
    } else if (inner >= 0) {
        chosen = inner;
    }

    if (chosen < 0)
        return;   // nothing to decide from yet; the next delivery may say more

    axisColumn_ = chosen;
    // Only a decision backed by observed motion is final. A first delivery of one
    // row sees no motion at all, and freezing Outer_Iter there would give the whole
    // run a flat axis that the offline reader would never have produced.
    if (certain)
        axisResolved_ = true;
    if (!warning.isEmpty() && update->warnings.isEmpty())
        update->warnings.append(warning);
}

int MonitorFileTailSource::builtInIndex(const QString &name) const
{
    for (int i = 0; i < columns_.size(); ++i) {
        if (columns_.at(i).key.compare(name, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

} // namespace monitor
} // namespace aerosim