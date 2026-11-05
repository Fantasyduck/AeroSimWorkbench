#include "MonitorLogReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <cmath>
#include <limits>

namespace aerosim {
namespace monitor {
namespace {

QString trimmedFieldImpl(const QString &raw)
{
    QString text = raw.trimmed();
    if (text.size() >= 2 && text.startsWith(QLatin1Char('"')) && text.endsWith(QLatin1Char('"')))
        text = text.mid(1, text.size() - 2).trimmed();
    return text;
}

// Splits one CSV line, honouring double quotes around the header names. The
// solver writes padded names such as  ,    "rms[Rho]"    ,  so the quote state
// only decides where a field ends; trimming happens afterwards.
QStringList splitCsvLineImpl(const QString &line)
{
    QStringList fields;
    QString current;
    bool quoted = false;
    for (const QChar ch : line) {
        if (ch == QLatin1Char('"')) {
            quoted = !quoted;
            current.append(ch);
        } else if (ch == QLatin1Char(',') && !quoted) {
            fields.append(current);
            current.clear();
        } else {
            current.append(ch);
        }
    }
    fields.append(current);
    return fields;
}

bool parseNumber(const QString &text, double *out)
{
    if (text.isEmpty())
        return false;
    bool ok = false;
    const double value = text.toDouble(&ok);
    if (!ok || !std::isfinite(value))
        return false;
    *out = value;
    return true;
}

} // namespace

bool MonitorLogReader::looksLikeResidual(const QString &columnKey)
{
    const QString key = columnKey.trimmed();
    return key.startsWith(QLatin1String("rms"), Qt::CaseInsensitive)
        || key.contains(QLatin1String("residual"), Qt::CaseInsensitive)
        || key.startsWith(QLatin1String("RMS["), Qt::CaseInsensitive);
}

QString MonitorLogReader::unitForColumn(const QString &columnKey)
{
    const QString key = columnKey.trimmed();
    if (looksLikeResidual(key))
        return QStringLiteral("-");
    static const QSet<QString> coefficientNames = {
        QStringLiteral("CD"), QStringLiteral("CL"), QStringLiteral("CSF"),
        QStringLiteral("CMx"), QStringLiteral("CMy"), QStringLiteral("CMz"),
        QStringLiteral("CFx"), QStringLiteral("CFy"), QStringLiteral("CFz"),
        QStringLiteral("CEff"), QStringLiteral("CX"), QStringLiteral("CY"),
        QStringLiteral("CZ")};
    if (coefficientNames.contains(key))
        return QStringLiteral("-");
    if (key.startsWith(QLatin1String("Coeff"), Qt::CaseInsensitive))
        return QStringLiteral("-");
    if (key.compare(QLatin1String("Time_Iter"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("iter");
    return QString();
}

bool MonitorLogReader::looksLikeAxis(const QString &columnKey)
{
    const QString key = columnKey.trimmed();
    return key.compare(QLatin1String("Time_Iter"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Outer_Iter"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Inner_Iter"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Iteration"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Iter"), Qt::CaseInsensitive) == 0;
}

MonitorColumn MonitorLogReader::classifyColumn(const QString &columnKey)
{
    MonitorColumn column;
    column.key = columnKey.trimmed();
    column.unit = unitForColumn(column.key);
    column.isResidual = looksLikeResidual(column.key);
    column.isAxis = looksLikeAxis(column.key);
    return column;
}

QStringList MonitorLogReader::splitCsvLine(const QString &line)
{
    return splitCsvLineImpl(line);
}

QString MonitorLogReader::trimmedField(const QString &raw)
{
    return trimmedFieldImpl(raw);
}

QString MonitorLogReader::datasetIdForPath(const QString &path)
{
    // The dataset id has to be stable and readable in the tree, so it is the
    // file stem alone. Qualifying it with the parent directory made two files
    // with the same name indistinguishable from their label only.
    return QFileInfo(path).completeBaseName();
}

bool MonitorLogReader::canRead(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.size() <= 0)
        return false;
    if (info.suffix().compare(QLatin1String("csv"), Qt::CaseInsensitive) != 0)
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QString head = QString::fromUtf8(file.read(4096));
    file.close();

    const QStringList lines = head.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    if (lines.isEmpty())
        return false;
    const QStringList fields = splitCsvLineImpl(lines.first());
    int usable = 0;
    for (const QString &field : fields) {
        const QString name = trimmedFieldImpl(field);
        if (looksLikeAxis(name) || looksLikeResidual(name))
            ++usable;
    }
    // A solver history header is recognised by its index or residual columns;
    // an aerodynamic-coefficient CSV such as tests/fixtures/.../curves.csv is not
    // a monitor history and must not be claimed by this reader.
    return usable > 0;
}

bool MonitorLogReader::read(const QString &path, MonitorLog *out, QString *error)
{
    if (error)
        error->clear();
    if (!out) {
        if (error)
            *error = QStringLiteral("内部错误：未提供 MonitorLog 输出对象。");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("无法打开文件：") + path;
        return false;
    }
    const QByteArray content = file.readAll();
    file.close();
    return readCsv(content, path, out, error);
}

bool MonitorLogReader::readCsv(const QByteArray &content, const QString &sourcePath,
                               MonitorLog *out, QString *error)
{
    if (error)
        error->clear();
    if (!out) {
        if (error)
            *error = QStringLiteral("内部错误：未提供 MonitorLog 输出对象。");
        return false;
    }
    MonitorLog log;
    log.sourcePath = sourcePath;
    log.datasetId = datasetIdForPath(sourcePath);

    if (content.isEmpty()) {
        if (error)
            *error = QStringLiteral("文件为空，没有可解析的监控序列。");
        return false;
    }
    if (content.contains('\0')) {
        if (error)
            *error = QStringLiteral("文件包含二进制内容，不是纯文本监控日志。");
        return false;
    }

    QString text = QString::fromUtf8(content);
    if (text.contains(QChar(0xFFFD))) {
        if (error)
            *error = QStringLiteral("文件不是有效的 UTF-8 文本，请先转换为 UTF-8 后再导入。");
        return false;
    }
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);

    const QStringList rawLines = text.split(QLatin1Char('\n'));
    QStringList lines;
    lines.reserve(rawLines.size());
    for (const QString &line : rawLines) {
        QString value = line;
        if (value.endsWith(QLatin1Char('\r')))
            value.chop(1);
        if (!value.trimmed().isEmpty())
            lines.append(value);
    }
    if (lines.isEmpty()) {
        if (error)
            *error = QStringLiteral("文件为空，没有可解析的监控序列。");
        return false;
    }

    // --- header ---
    const QStringList headerFields = splitCsvLineImpl(lines.first());
    for (const QString &field : headerFields) {
        const QString key = trimmedFieldImpl(field);
        if (key.isEmpty())
            continue;
        MonitorColumn column;
        column.key = key;
        column.unit = unitForColumn(key);
        column.isResidual = looksLikeResidual(key);
        column.isAxis = looksLikeAxis(key);
        if (!log.columns.isEmpty() && log.columns.last().key == key) {
            log.warnings.append(QStringLiteral("表头出现重复列名 %1，已只保留第一次出现。").arg(key));
            continue;
        }
        log.columns.append(column);
    }
    if (log.columns.isEmpty()) {
        if (error)
            *error = QStringLiteral("表头为空或不含可识别的列名。");
        return false;
    }

    // A generic CSV (a,b / 1,2) has no index, residual or coefficient column.
    // Accepting it would draw an arbitrary column against a row index and call
    // it a solver history, so it is refused with a readable reason instead.
    int recognisedColumns = 0;
    for (const MonitorColumn &column : log.columns) {
        if (column.isAxis || column.isResidual || !column.unit.isEmpty())
            ++recognisedColumns;
    }
    if (recognisedColumns == 0) {
        if (error)
            *error = QStringLiteral("文件中没有可识别的监控列（迭代步、残差或气动系数），请确认这是求解器输出的 history 文件。");
        return false;
    }

    const int columnCount = log.columns.size();
    QVector<QVector<double>> raw(columnCount);
    for (QVector<double> &column : raw)
        column.reserve(lines.size() - 1);

    int skippedRows = 0;
    int raggedRows = 0;
    for (int row = 1; row < lines.size(); ++row) {
        const QStringList fields = splitCsvLineImpl(lines.at(row));
        if (fields.size() != columnCount) {
            ++raggedRows;
            if (fields.size() < columnCount) {
                ++skippedRows;
                continue;
            }
        }
        for (int index = 0; index < columnCount; ++index) {
            const QString cell = index < fields.size() ? fields.at(index) : QString();
            double value = 0.0;
            if (!parseNumber(cell.trimmed(), &value)) {
                if (index == 0)
                    ++skippedRows;
                value = std::numeric_limits<double>::quiet_NaN();
            }
            raw[index].append(value);
        }
    }
    const int rowCount = raw.isEmpty() ? 0 : raw.first().size();
    if (rowCount == 0) {
        if (error)
            *error = QStringLiteral("文件中没有可解析的数据行。");
        return false;
    }
    if (skippedRows > 0)
        log.warnings.append(QStringLiteral("已跳过 %1 行无法解析的数据行。").arg(skippedRows));
    if (raggedRows > 0)
        log.warnings.append(QStringLiteral("%1 行的字段数与表头不一致，已按表头列数对齐。").arg(raggedRows));

    // --- columns -> values ---
    QStringList unitUnknown;
    int axisColumns = 0;
    for (int index = 0; index < columnCount; ++index) {
        const MonitorColumn &column = log.columns.at(index);
        if (column.isAxis)
            ++axisColumns;
        else if (column.unit.isEmpty())
            unitUnknown.append(column.key);
        log.values.insert(column.key, raw.at(index));
    }
    if (axisColumns == 0)
        log.warnings.append(QStringLiteral("文件没有迭代步或时间索引列，横轴将使用行序号。"));
    if (!unitUnknown.isEmpty())
        log.warnings.append(QStringLiteral("下列列的单位无法推断，按无量纲处理：%1").arg(unitUnknown.join(QStringLiteral("、"))));

    // --- axes ---
    const auto columnIndex = [&log](const QString &key) {
        for (int index = 0; index < log.columns.size(); ++index)
            if (log.columns.at(index).key == key)
                return index;
        return -1;
    };
    const auto progressionOf = [&raw](int index) -> qint64 {
        if (index < 0 || index >= raw.size() || raw.at(index).isEmpty())
            return 0;
        const QVector<double> &column = raw.at(index);
        qint64 changes = 0;
        for (int row = 1; row < column.size(); ++row)
            if (column.at(row) != column.at(0))
                ++changes;
        return changes;
    };
    const auto asIntegers = [&raw](int index) -> QVector<qint64> {
        QVector<qint64> result;
        if (index < 0 || index >= raw.size())
            return result;
        const QVector<double> &column = raw.at(index);
        result.reserve(column.size());
        for (const double value : column)
            result.append(std::isfinite(value) ? static_cast<qint64>(std::llround(value)) : 0);
        return result;
    };

    const int outerIndex = columnIndex(QStringLiteral("Outer_Iter"));
    const int innerIndex = columnIndex(QStringLiteral("Inner_Iter"));
    const int timeIndexColumn = columnIndex(QStringLiteral("Time_Iter"));
    log.outerIterationProgression = progressionOf(outerIndex);
    log.innerIterationProgression = progressionOf(innerIndex);

    if (outerIndex >= 0 && log.outerIterationProgression > 0) {
        log.iterationStep = asIntegers(outerIndex);
    } else if (innerIndex >= 0 && log.innerIterationProgression > 0) {
        log.iterationStep = asIntegers(innerIndex);
        log.warnings.append(QStringLiteral("Outer_Iter 未推进，迭代步横轴回退为 Inner_Iter。"));
    } else if (outerIndex >= 0) {
        log.iterationStep = asIntegers(outerIndex);
    } else if (innerIndex >= 0) {
        log.iterationStep = asIntegers(innerIndex);
    } else {
        log.iterationStep.reserve(rowCount);
        for (int row = 0; row < rowCount; ++row)
            log.iterationStep.append(row);
    }
    if (timeIndexColumn >= 0)
        log.timeIndex = asIntegers(timeIndexColumn);

    bool hasPlottable = false;
    for (const MonitorColumn &column : log.columns) {
        if (!column.isAxis && log.values.value(column.key).size() == rowCount) {
            hasPlottable = true;
            break;
        }
    }
    if (!hasPlottable) {
        if (error)
            *error = QStringLiteral("文件只有索引列，没有可绘制的数值列。");
        return false;
    }

    *out = log;
    return true;
}

} // namespace monitor
} // namespace aerosim
