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

// The number parsing both readers use. Kept as a free function so the public
// static below can be a one-line forward without a second copy of the rules.
bool parseNumberImpl(const QString &text, double *out)
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

// ---------------------------------------------------------------------------
// Tecplot ASCII framing
//
// This is the format the reference implementation (XCFD) feeds its convergence
// curves from: it reads "*.tec" through VTK's vtkTecplotReader and only uses CSV
// for its table widgets. The framing is a TITLE record, a VARIABLES record that
// names the columns, then one or more ZONE records that say how many points the
// zone holds and how their values are laid out, followed by the numbers.
//
// Recognising a file by the same head records the reference reader keys on
// (TITLE / VARIABLES / ZONE) means a file is accepted or rejected for the same
// reason on both sides.
// ---------------------------------------------------------------------------

// The leading word of a record line, upper-cased: "variables = ..." -> VARIABLES.
QString tecFirstWord(const QString &line)
{
    int index = 0;
    while (index < line.size() && line.at(index).isSpace())
        ++index;
    const int start = index;
    while (index < line.size()) {
        const QChar ch = line.at(index);
        if (ch.isSpace() || ch == QLatin1Char('=') || ch == QLatin1Char(','))
            break;
        ++index;
    }
    return line.mid(start, index - start).toUpper();
}

// Records Tecplot may write besides the three this reader acts on. A line that
// starts with one of these ends a VARIABLES continuation or a data block, which
// is how the parser tells a multi-line record from the numbers under it.
bool isTecplotKeyword(const QString &word)
{
    static const QSet<QString> keywords = {
        QStringLiteral("TITLE"), QStringLiteral("VARIABLES"), QStringLiteral("ZONE"),
        QStringLiteral("DATASETAUXDATA"), QStringLiteral("VARSHARELIST"),
        QStringLiteral("CONNECTIVITYSHAREZONE"), QStringLiteral("GEOMETRY"),
        QStringLiteral("TEXT"), QStringLiteral("CUSTOMLABELS"), QStringLiteral("AUXDATA"),
        QStringLiteral("RESULTS")};
    return keywords.contains(word);
}

// VARIABLES value -> column names. Names are either quoted ("rms[Rho]") or bare,
// and separated by commas or whitespace; a quoted name may contain either.
QStringList tecSplitNames(const QString &text)
{
    QStringList names;
    QString current;
    bool quoted = false;
    for (const QChar ch : text) {
        if (ch == QLatin1Char('"')) {
            quoted = !quoted;
            continue;
        }
        if (!quoted && (ch == QLatin1Char(',') || ch.isSpace())) {
            const QString name = current.trimmed();
            if (!name.isEmpty())
                names.append(name);
            current.clear();
            continue;
        }
        current.append(ch);
    }
    const QString name = current.trimmed();
    if (!name.isEmpty())
        names.append(name);
    return names;
}

// One data cell as a number. Tecplot files written by a Fortran solver use the
// Fortran exponent form (1.0D+00); Qt's parser only knows 'E', so the 'D' is
// rewritten first. Only a 'D' that sits where an exponent marker can sit is
// touched, so a column named "CD" is never mangled.
bool tecParseNumber(const QString &token, double *out)
{
    const QString text = trimmedFieldImpl(token);
    if (text.isEmpty())
        return false;
    QString normalised = text;
    for (int index = 1; index + 1 < normalised.size(); ++index) {
        const QChar ch = normalised.at(index);
        if (ch != QLatin1Char('D') && ch != QLatin1Char('d'))
            continue;
        if (!normalised.at(index - 1).isDigit())
            continue;
        const QChar next = normalised.at(index + 1);
        if (!next.isDigit() && next != QLatin1Char('+') && next != QLatin1Char('-'))
            continue;
        normalised[index] = QLatin1Char('E');
        break;
    }
    return parseNumberImpl(normalised, out);
}

// The header of one ZONE record, as far as the reader needs it.
struct TecplotZone {
    qint64 i = 0;
    qint64 j = 0;
    qint64 k = 0;
    qint64 n = -1;
    bool hasI = false;
    bool hasJ = false;
    bool hasK = false;
    bool blockPacking = false;   // DATAPACKING=BLOCK or F=FBLOCK
    bool feBlock = false;        // F=FEBLOCK: element-major, not a history layout
    QString zoneType;            // empty means ORDERED
    QString title;
};

void tecParseZoneHeader(const QString &header, TecplotZone *zone)
{
    QStringList parts;
    QString current;
    bool quoted = false;
    for (const QChar ch : header) {
        if (ch == QLatin1Char('"')) {
            quoted = !quoted;
            current.append(ch);
            continue;
        }
        if (ch == QLatin1Char(',') && !quoted) {
            parts.append(current);
            current.clear();
            continue;
        }
        current.append(ch);
    }
    parts.append(current);

    // The first part still carries the record word itself ("ZONE T=...").
    if (!parts.isEmpty()) {
        const QString trimmed = parts.first().trimmed();
        if (trimmed.toUpper().startsWith(QLatin1String("ZONE")))
            parts[0] = trimmed.mid(4);
    }

    for (const QString &part : parts) {
        const int equals = part.indexOf(QLatin1Char('='));
        if (equals < 0)
            continue;
        const QString key = part.left(equals).trimmed().toUpper();
        const QString value = trimmedFieldImpl(part.mid(equals + 1));
        const QString upper = value.toUpper();
        bool ok = false;
        if (key == QLatin1String("I")) {
            zone->i = value.toLongLong(&ok);
            zone->hasI = ok;
        } else if (key == QLatin1String("J")) {
            zone->j = value.toLongLong(&ok);
            zone->hasJ = ok;
        } else if (key == QLatin1String("K")) {
            zone->k = value.toLongLong(&ok);
            zone->hasK = ok;
        } else if (key == QLatin1String("N")) {
            zone->n = value.toLongLong(&ok);
            if (!ok)
                zone->n = -1;
        } else if (key == QLatin1String("DATAPACKING")) {
            zone->blockPacking = upper == QLatin1String("BLOCK");
        } else if (key == QLatin1String("F")) {
            zone->feBlock = upper == QLatin1String("FEBLOCK");
            zone->blockPacking = zone->feBlock || upper == QLatin1String("BLOCK")
                                 || upper == QLatin1String("FEBLOCK");
        } else if (key == QLatin1String("ZONETYPE")) {
            zone->zoneType = upper;
        } else if (key == QLatin1String("T")) {
            zone->title = value;
        }
    }
}

bool assembleLog(MonitorLog *log, const QVector<QVector<double>> &raw,
                 MonitorLog *out, QString *error);

// The value part of a record line: everything after the record word and the
// optional '=' sign. "VARIABLES = \"CD\", \"CL\"" -> "\"CD\", \"CL\"".
QString tecRecordValue(const QString &line)
{
    int index = 0;
    while (index < line.size() && (line.at(index).isSpace() || line.at(index) == QLatin1Char('=')))
        ++index;
    while (index < line.size() && !line.at(index).isSpace()
           && line.at(index) != QLatin1Char('=') && line.at(index) != QLatin1Char(','))
        ++index;
    while (index < line.size() && (line.at(index).isSpace() || line.at(index) == QLatin1Char('=')))
        ++index;
    return line.mid(index).trimmed();
}

// Whether a line opens with a number rather than with a record word. Used to end
// a multi-line header record, so a file that forgot its ZONE record cannot make
// the parser swallow the data as if it were still reading a name list.
bool tecLooksNumeric(const QString &line)
{
    for (const QChar ch : line) {
        if (ch.isSpace())
            continue;
        return ch.isDigit() || ch == QLatin1Char('-') || ch == QLatin1Char('+')
               || ch == QLatin1Char('.');
    }
    return false;
}

// One data line as tokens. Tecplot separates values with whitespace or commas,
// unlike the CSV reader's commas only -- the format is whitespace-delimited, so
// splitCsvLine() cannot be used here.
QStringList tecSplitTokens(const QString &line)
{
    QStringList tokens;
    int index = 0;
    while (index < line.size()) {
        const QChar ch = line.at(index);
        if (ch.isSpace() || ch == QLatin1Char(',')) {
            ++index;
            continue;
        }
        const int start = index;
        while (index < line.size() && !line.at(index).isSpace()
               && line.at(index) != QLatin1Char(','))
            ++index;
        tokens.append(line.mid(start, index - start));
    }
    return tokens;
}

// Whether a head of a file is Tecplot ASCII at all.
//
// The record word is matched together with the character after it: a CSV whose
// first column happens to be called ZONE writes "ZONE,CD,CL" and must not be
// mistaken for a Tecplot record.
bool tecLooksLikeTecplotLines(const QStringList &lines)
{
    const int limit = qMin(lines.size(), 20);
    for (int index = 0; index < limit; ++index) {
        const QString upper = lines.at(index).trimmed().toUpper();
        if (upper.isEmpty())
            continue;
        const auto isRecord = [&upper](const char *word) {
            const QString prefix = QString::fromLatin1(word);
            if (!upper.startsWith(prefix))
                return false;
            if (upper.size() == prefix.size())
                return true;
            const QChar next = upper.at(prefix.size());
            return next.isSpace() || next == QLatin1Char('=');
        };
        if (isRecord("TITLE") || isRecord("VARIABLES") || isRecord("ZONE"))
            return true;
    }
    return false;
}

// The column names a VARIABLES record declares, with its continuation lines
// folded in. A long variable list is routinely written over several lines, and
// only the first of them starts with the record word.
QStringList tecVariableNames(const QStringList &lines)
{
    for (int index = 0; index < lines.size(); ++index) {
        if (tecFirstWord(lines.at(index)) != QLatin1String("VARIABLES"))
            continue;
        QString buffer = tecRecordValue(lines.at(index));
        for (int next = index + 1; next < lines.size(); ++next) {
            const QString &line = lines.at(next);
            if (isTecplotKeyword(tecFirstWord(line)) || tecLooksNumeric(line))
                break;
            buffer += QLatin1Char(' ') + line;
        }
        return tecSplitNames(buffer);
    }
    return QStringList();
}

// A ZONE record's own attributes, so a header written over several lines is
// joined before it is parsed.
bool tecIsZoneAttribute(const QString &word)
{
    static const QSet<QString> attributes = {
        QStringLiteral("I"), QStringLiteral("J"), QStringLiteral("K"), QStringLiteral("N"),
        QStringLiteral("F"), QStringLiteral("T"), QStringLiteral("DATAPACKING"),
        QStringLiteral("ZONETYPE"), QStringLiteral("STRANDID"), QStringLiteral("SOLUTIONTIME"),
        QStringLiteral("VARLOCATION"), QStringLiteral("VARSHARELIST"),
        QStringLiteral("CONNECTIVITYSHAREZONE"), QStringLiteral("D"), QStringLiteral("DT"),
        QStringLiteral("C")};
    return attributes.contains(word);
}

// Names -> columns, with the duplicate renaming the CSV header path applies.
// Shared so a Tecplot file that declares the same variable twice is handled the
// same way, and warned about in the same words, as a CSV header that does.
QVector<MonitorColumn> columnsFromNames(const QStringList &names, QStringList *warnings)
{
    QVector<MonitorColumn> columns;
    QStringList usedKeys;
    columns.reserve(names.size());
    for (const QString &name : names) {
        MonitorColumn column = MonitorLogReader::classifyColumn(name);
        // Classified under its own name so a renamed duplicate keeps the unit and
        // the residual/axis meaning of the column it actually is; only the key,
        // which has to be unique, carries the suffix.
        QString key = column.key;
        int suffix = 1;
        while (usedKeys.contains(key, Qt::CaseInsensitive)) {
            ++suffix;
            key = column.key + QLatin1Char('#') + QString::number(suffix);
        }
        if (key != column.key) {
            if (warnings) {
                warnings->append(QStringLiteral("表头列名 %1 出现了 %2 次，第 %2 次已改名为 %3，以免两列数据互相覆盖。")
                                     .arg(column.key, QString::number(suffix), key));
            }
            column.key = key;
        }
        usedKeys.append(key);
        columns.append(column);
    }
    return columns;
}

// Tecplot ASCII -> the column matrix, one ZONE at a time.
//
// A history file is ORDERED with I = the number of steps, but a solver may also
// append a new ZONE per restart, so the zones are concatenated in file order:
// that is the same series continued, not several series. FE zones carry
// element-connectivity layout instead of a per-step record and are refused by
// name rather than silently misread.
bool parseTecplot(MonitorLog *log, const QStringList &lines, MonitorLog *out, QString *error)
{
    const QStringList names = tecVariableNames(lines);
    if (names.isEmpty()) {
        if (error)
            *error = QStringLiteral("Tecplot 文件缺少 VARIABLES 记录，无法确定列名。");
        return false;
    }
    log->columns = columnsFromNames(names, &log->warnings);
    if (log->columns.isEmpty()) {
        if (error)
            *error = QStringLiteral("表头为空或不含可识别的列名。");
        return false;
    }
    if (!MonitorLogReader::hasRecognisedColumns(log->columns)) {
        if (error)
            *error = QStringLiteral("文件中没有可识别的监控列（迭代步、残差或气动系数），请确认这是求解器输出的 history 文件。");
        return false;
    }

    const int columnCount = names.size();
    QVector<QVector<double>> raw(columnCount);

    int zoneCount = 0;
    int skippedZones = 0;
    int unparsableValues = 0;

    int index = 0;
    // Records before the first ZONE (TITLE, VARIABLES, and any of the auxiliary
    // records this reader ignores) describe the file rather than its data.
    while (index < lines.size() && tecFirstWord(lines.at(index)) != QLatin1String("ZONE"))
        ++index;
    if (index >= lines.size()) {
        if (error)
            *error = QStringLiteral("Tecplot 文件没有 ZONE 记录，无法确定数据从哪里开始。");
        return false;
    }

    while (index < lines.size()) {
        if (tecFirstWord(lines.at(index)) != QLatin1String("ZONE")) {
            ++index;
            continue;
        }

        // A ZONE header can be written over several lines: the record word starts
        // the first line, and the remaining attributes continue it.
        QString header = lines.at(index);
        ++index;
        while (index < lines.size()) {
            const QString word = tecFirstWord(lines.at(index));
            if (isTecplotKeyword(word) || tecLooksNumeric(lines.at(index)))
                break;
            if (!tecIsZoneAttribute(word))
                break;
            header += QLatin1Char(',') + lines.at(index);
            ++index;
        }

        TecplotZone zone;
        tecParseZoneHeader(header, &zone);

        // The data of this zone: everything up to the next record word.
        QVector<QString> tokens;
        while (index < lines.size()) {
            const QString word = tecFirstWord(lines.at(index));
            if (isTecplotKeyword(word))
                break;
            const QStringList fields = tecSplitTokens(lines.at(index));
            for (const QString &field : fields)
                tokens.append(field);
            ++index;
        }

        ++zoneCount;

        if (!zone.zoneType.isEmpty() && zone.zoneType != QLatin1String("ORDERED")) {
            ++skippedZones;
            log->warnings.append(QStringLiteral("第 %1 个 ZONE 是 %2 类型（非 ORDERED），不是逐步记录的历史布局，已跳过。")
                                     .arg(QString::number(zoneCount), zone.zoneType));
            continue;
        }

        qint64 pointCount = -1;
        if (zone.hasI) {
            const qint64 j = zone.hasJ && zone.j > 1 ? zone.j : 1;
            const qint64 k = zone.hasK && zone.k > 1 ? zone.k : 1;
            pointCount = zone.i * j * k;
        } else if (zone.n > 0) {
            pointCount = zone.n;
        }
        // A zone with no declared extent is read by counting instead, which is
        // the only reading that cannot invent a shape the file does not state.
        if (pointCount <= 0)
            pointCount = columnCount > 0 ? tokens.size() / columnCount : 0;

        if (pointCount <= 0) {
            ++skippedZones;
            log->warnings.append(QStringLiteral("第 %1 个 ZONE 没有声明数据点数，也没有可计数的数据行，已跳过。")
                                     .arg(QString::number(zoneCount)));
            continue;
        }

        const qint64 expected = pointCount * columnCount;
        if (tokens.size() < expected) {
            // Short or long, the zone is refused rather than padded: filling the
            // gap would draw a step the solver never wrote.
            ++skippedZones;
            log->warnings.append(QStringLiteral("第 %1 个 ZONE 声明的数据量与文件内容不符（%2 点 × %3 列共需 %4 个数值，实际 %5 个），已跳过该 ZONE。")
                                     .arg(QString::number(zoneCount), QString::number(pointCount),
                                          QString::number(columnCount), QString::number(expected),
                                          QString::number(tokens.size())));
            continue;
        }

        for (qint64 point = 0; point < pointCount; ++point) {
            for (int column = 0; column < columnCount; ++column) {
                const qint64 offset = zone.blockPacking
                                          ? static_cast<qint64>(column) * pointCount + point
                                          : point * columnCount + column;
                double value = 0.0;
                if (offset >= tokens.size() || !tecParseNumber(tokens.at(offset), &value)) {
                    ++unparsableValues;
                    value = std::numeric_limits<double>::quiet_NaN();
                }
                raw[column].append(value);
            }
        }
    }

    if (raw.isEmpty() || raw.first().isEmpty()) {
        if (error)
            *error = QStringLiteral("Tecplot 文件中没有可解析的数据行。");
        return false;
    }
    if (skippedZones > 0) {
        log->warnings.append(QStringLiteral("共跳过 %1 个 ZONE，其余 ZONE 已顺序拼接为同一条序列。")
                                 .arg(skippedZones));
    }
    if (unparsableValues > 0) {
        log->warnings.append(QStringLiteral("有 %1 个数值无法解析，已按缺失处理。").arg(unparsableValues));
    }

    return assembleLog(log, raw, out, error);
}

// Raw bytes -> lines, with the encoding doubts settled once for both readers.
//
// Shared so a CSV and a Tecplot file are rejected for the same reasons in the
// same words; a Tecplot file that is really a compressed .plt archive has to
// fail exactly as a binary CSV would.
bool decodeMonitorText(const QByteArray &content, QStringList *lines, QString *error)
{
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
    lines->clear();
    lines->reserve(rawLines.size());
    for (const QString &line : rawLines) {
        QString value = line;
        if (value.endsWith(QLatin1Char('\r')))
            value.chop(1);
        if (!value.trimmed().isEmpty())
            lines->append(value);
    }
    if (lines->isEmpty()) {
        if (error)
            *error = QStringLiteral("文件为空，没有可解析的监控序列。");
        return false;
    }
    return true;
}

// Column matrix -> the finished MonitorLog: values, axes, and the warnings that
// explain any fallback.
//
// Shared by the CSV and Tecplot paths rather than duplicated: the axis chain
// (Outer_Iter, then Inner_Iter, then a registered primary axis, then the row
// index) and the "no plottable series" refusal are the reader's rules, and two
// copies of them would let the two formats disagree about which column is the
// x axis. Returns false only for a file with nothing to plot, and always leaves
// *error set in that case.
bool assembleLog(MonitorLog *log, const QVector<QVector<double>> &raw,
                 MonitorLog *out, QString *error)
{
    const int columnCount = log->columns.size();
    const int rowCount = raw.isEmpty() ? 0 : raw.first().size();
    if (rowCount == 0) {
        if (error)
            *error = QStringLiteral("文件中没有可解析的数据行。");
        return false;
    }

    // --- columns -> values ---
    QStringList unitUnknown;
    int axisColumns = 0;
    for (int index = 0; index < columnCount; ++index) {
        const MonitorColumn &column = log->columns.at(index);
        if (column.isAxis)
            ++axisColumns;
        else if (column.unit.isEmpty())
            unitUnknown.append(column.key);
        log->values.insert(column.key, raw.at(index));
    }
    if (axisColumns == 0)
        log->warnings.append(QStringLiteral("文件没有迭代步或时间索引列，横轴将使用行序号。"));
    if (!unitUnknown.isEmpty())
        log->warnings.append(QStringLiteral("下列列的单位无法推断，按无量纲处理：%1").arg(unitUnknown.join(QStringLiteral("、"))));

    // --- axes ---
    const auto columnIndex = [log](const QString &key) {
        for (int index = 0; index < log->columns.size(); ++index)
            if (log->columns.at(index).key == key)
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
    log->outerIterationProgression = progressionOf(outerIndex);
    log->innerIterationProgression = progressionOf(innerIndex);

    // A column a registered rule calls the primary axis, found before the
    // built-in chain is consulted so the built-in names keep precedence.
    int primaryAxisIndex = -1;
    {
        const MonitorColumnRegistry &registry = MonitorColumnRegistry::instance();
        for (int index = 0; index < log->columns.size() && primaryAxisIndex < 0; ++index) {
            const MonitorColumn &column = log->columns.at(index);
            if (column.isAxis && registry.hasPrimaryAxisRule(column.key))
                primaryAxisIndex = index;
        }
    }

    if (outerIndex >= 0 && log->outerIterationProgression > 0) {
        log->iterationStep = asIntegers(outerIndex);
    } else if (innerIndex >= 0 && log->innerIterationProgression > 0) {
        log->iterationStep = asIntegers(innerIndex);
        log->warnings.append(QStringLiteral("Outer_Iter 未推进，迭代步横轴回退为 Inner_Iter。"));
    } else if (outerIndex >= 0) {
        log->iterationStep = asIntegers(outerIndex);
    } else if (innerIndex >= 0) {
        log->iterationStep = asIntegers(innerIndex);
    } else if (primaryAxisIndex >= 0) {
        // A registered convention's own step column, tried before giving up and
        // using the row index: a row index is a fabrication, the column is data.
        log->iterationStep = asIntegers(primaryAxisIndex);
        log->warnings.append(QStringLiteral("迭代步横轴取自注册规则识别的列 %1。")
                                 .arg(log->columns.at(primaryAxisIndex).key));
    } else {
        log->iterationStep.reserve(rowCount);
        for (int row = 0; row < rowCount; ++row)
            log->iterationStep.append(row);
    }
    if (timeIndexColumn >= 0)
        log->timeIndex = asIntegers(timeIndexColumn);

    bool hasPlottable = false;
    for (const MonitorColumn &column : log->columns) {
        if (!column.isAxis && log->values.value(column.key).size() == rowCount) {
            hasPlottable = true;
            break;
        }
    }
    if (!hasPlottable) {
        if (error)
            *error = QStringLiteral("文件只有索引列，没有可绘制的数值列。");
        return false;
    }

    *out = *log;
    return true;
}

} // namespace

MonitorColumnRegistry &MonitorColumnRegistry::instance()
{
    // Function-local static: the registry has to exist before the first reader
    // runs, and a namespace-scope object would depend on translation-unit
    // initialisation order that nothing here controls.
    static MonitorColumnRegistry registry;
    return registry;
}

void MonitorColumnRegistry::registerRules(const MonitorColumnRules &rules)
{
    if (rules.id.isEmpty())
        return;
    for (int index = 0; index < rules_.size(); ++index) {
        if (rules_.at(index).id == rules.id) {
            rules_.replace(index, rules);
            return;
        }
    }
    rules_.append(rules);
}

void MonitorColumnRegistry::clearRules()
{
    rules_.clear();
}

QStringList MonitorColumnRegistry::registeredIds() const
{
    QStringList ids;
    ids.reserve(rules_.size());
    for (const MonitorColumnRules &rules : rules_)
        ids.append(rules.id);
    return ids;
}

bool MonitorColumnRegistry::hasPrimaryAxisRule(const QString &columnKey) const
{
    for (const MonitorColumnRules &rules : rules_) {
        if (rules.isPrimaryAxis && rules.isPrimaryAxis(columnKey))
            return true;
    }
    return false;
}

bool MonitorColumnRegistry::hasResidualRule(const QString &columnKey) const
{
    for (const MonitorColumnRules &rules : rules_) {
        if (rules.isResidual && rules.isResidual(columnKey))
            return true;
    }
    return false;
}

bool MonitorColumnRegistry::hasAxisRule(const QString &columnKey) const
{
    for (const MonitorColumnRules &rules : rules_) {
        if (rules.isAxis && rules.isAxis(columnKey))
            return true;
    }
    return false;
}

QString MonitorColumnRegistry::unitFromRules(const QString &columnKey) const
{
    for (const MonitorColumnRules &rules : rules_) {
        if (!rules.unitFor)
            continue;
        const QString unit = rules.unitFor(columnKey);
        if (!unit.isEmpty())
            return unit;
    }
    return QString();
}

bool MonitorLogReader::looksLikeResidual(const QString &columnKey)
{
    const QString key = columnKey.trimmed();
    // Built-in rules first. A registered convention only gets the names the
    // built-ins do not claim, so adding one can never reinterpret an existing
    // SU2 header.
    if (key.startsWith(QLatin1String("rms"), Qt::CaseInsensitive)
        || key.contains(QLatin1String("residual"), Qt::CaseInsensitive)
        || key.startsWith(QLatin1String("RMS["), Qt::CaseInsensitive))
        return true;
    return MonitorColumnRegistry::instance().hasResidualRule(key);
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
    return MonitorColumnRegistry::instance().unitFromRules(key);
}

bool MonitorLogReader::looksLikeAxis(const QString &columnKey)
{
    const QString key = columnKey.trimmed();
    if (key.compare(QLatin1String("Time_Iter"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Outer_Iter"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Inner_Iter"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Iteration"), Qt::CaseInsensitive) == 0
        || key.compare(QLatin1String("Iter"), Qt::CaseInsensitive) == 0)
        return true;
    return MonitorColumnRegistry::instance().hasAxisRule(key);
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

bool MonitorLogReader::hasRecognisedColumns(const QVector<MonitorColumn> &columns)
{
    // An index column, a residual, or a column whose unit we know all mean "this
    // is a solver history". A generic CSV has none of those and would otherwise
    // be drawn as a solver curve against a row index.
    for (const MonitorColumn &column : columns) {
        if (column.isAxis || column.isResidual || !column.unit.isEmpty())
            return true;
    }
    return false;
}

QStringList MonitorLogReader::splitCsvLine(const QString &line)
{
    return splitCsvLineImpl(line);
}

QString MonitorLogReader::trimmedField(const QString &raw)
{
    return trimmedFieldImpl(raw);
}

bool MonitorLogReader::parseNumericField(const QString &raw, double *out)
{
    // The quotes are stripped first: a solver that quotes its header usually
    // quotes its numbers too, and the offline path used to turn those cells into
    // NaN while the tail read them fine. Same cell, same value, either way in.
    return parseNumberImpl(trimmedFieldImpl(raw), out);
}

bool MonitorLogReader::admitDataRow(const QString &line, int columnCount,
                                    QStringList *aligned, bool *ragged)
{
    QVector<int> identical(columnCount);
    for (int index = 0; index < columnCount; ++index)
        identical[index] = index;
    return admitDataRow(line, columnCount, identical, aligned, ragged);
}

bool MonitorLogReader::admitDataRow(const QString &line, int columnCount,
                                    const QVector<int> &fieldToColumn,
                                    QStringList *aligned, bool *ragged)
{
    if (ragged)
        *ragged = false;
    if (columnCount <= 0 || fieldToColumn.isEmpty())
        return false;

    // The row must reach every header field that carries a column. The header's
    // own field count is the yardstick: comparing against the number of columns
    // instead reported every row of a header ending in a comma as ragged, and it
    // is what used to make a de-duplicated header misalign silently.
    int required = 0;
    for (int field = 0; field < fieldToColumn.size(); ++field) {
        if (fieldToColumn.at(field) >= 0)
            required = field + 1;
    }

    QStringList fields = splitCsvLineImpl(line);
    // A trailing separator writes no data, exactly as in the header, where the
    // trailing empty field is dropped from the map. Counting it here reported a
    // perfectly regular file as ragged on every single row.
    while (!fields.isEmpty() && trimmedFieldImpl(fields.last()).isEmpty())
        fields.removeLast();
    if (ragged)
        *ragged = fields.size() != fieldToColumn.size();
    if (fields.size() < required)
        return false;   // the row cannot be attributed to the columns

    if (aligned) {
        // Built by column, not by field: a header field that carries no column
        // leaves a hole rather than shifting its neighbours left.
        QStringList byColumn;
        for (int column = 0; column < columnCount; ++column)
            byColumn.append(QString());   // QStringList has neither resize() nor an int constructor in Qt 5.12
        for (int field = 0; field < fieldToColumn.size() && field < fields.size(); ++field) {
            const int column = fieldToColumn.at(field);
            if (column >= 0 && column < columnCount)
                byColumn[column] = fields.at(field);
        }
        *aligned = byColumn;
    }
    return true;
}

QVector<MonitorColumn> MonitorLogReader::parseHeaderColumns(const QString &headerLine,
                                                            QStringList *warnings,
                                                            QVector<int> *fieldToColumn)
{
    QVector<MonitorColumn> columns;
    QVector<int> mapping;
    QStringList usedKeys;

    const QStringList fields = splitCsvLineImpl(headerLine);
    mapping.reserve(fields.size());
    for (const QString &field : fields) {
        const QString name = trimmedFieldImpl(field);
        if (name.isEmpty()) {
            // A nameless field carries no column. Its position is still recorded so
            // the fields to its right keep their own alignment.
            mapping.append(-1);
            continue;
        }
        MonitorColumn column = classifyColumn(name);
        // Classified under its own name so a renamed duplicate keeps the unit and
        // the residual/axis meaning of the column it actually is; only the key,
        // which has to be unique, carries the suffix.
        QString key = column.key;
        int suffix = 1;
        while (usedKeys.contains(key, Qt::CaseInsensitive)) {
            ++suffix;
            key = column.key + QLatin1Char('#') + QString::number(suffix);
        }
        if (key != column.key) {
            if (warnings) {
                warnings->append(QStringLiteral("表头列名 %1 出现了 %2 次，第 %2 次已改名为 %3，以免两列数据互相覆盖。")
                                     .arg(column.key, QString::number(suffix), key));
            }
            column.key = key;
        }
        usedKeys.append(key);
        mapping.append(columns.size());
        columns.append(column);
    }

    // A header that ends with a separator declares a trailing empty field nobody
    // writes data for; keeping it in the map would demand a trailing comma on
    // every row and report files that are perfectly regular as ragged.
    while (!mapping.isEmpty() && mapping.last() < 0)
        mapping.removeLast();

    if (fieldToColumn)
        *fieldToColumn = mapping;
    return columns;
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
    // The extension is only a cheap pre-filter; what decides is the content.
    // .dat is written by both formats (SU2 history, Tecplot ASCII), so refusing
    // it by name would hide files the reader can parse, and accepting .tec
    // without looking would offer archive-format .plt binaries.
    const QString suffix = info.suffix();
    // .plt is deliberately absent: Tecplot's binary archive format is not text and
    // this reader cannot decode it, so offering it would only lead to a failure.
    static const QSet<QString> suffixes = {
        QStringLiteral("csv"), QStringLiteral("dat"), QStringLiteral("tec"),
        QStringLiteral("log")};
    if (!suffixes.contains(suffix.toLower()))
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QString head = QString::fromUtf8(file.read(4096));
    file.close();

    const QStringList lines = head.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    if (lines.isEmpty())
        return false;

    // Classify exactly as the reader would, then ask the same predicate. Probing
    // and reading now cannot disagree about what they accept: a file the probe
    // rejects but the reader accepts would let a UI hide a file it could actually
    // draw.
    if (tecLooksLikeTecplotLines(lines)) {
        const QStringList names = tecVariableNames(lines);
        if (names.isEmpty())
            return false;
        return hasRecognisedColumns(columnsFromNames(names, nullptr));
    }

    QVector<MonitorColumn> columns;
    const QStringList fields = splitCsvLineImpl(lines.first());
    columns.reserve(fields.size());
    for (const QString &field : fields) {
        const QString name = trimmedFieldImpl(field);
        if (name.isEmpty())
            continue;
        columns.append(classifyColumn(name));
    }
    return hasRecognisedColumns(columns);
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

    // Which reader parses the file is decided by its content, not its suffix: a
    // .dat is as likely to be Tecplot ASCII as a comma-separated history, and a
    // CSV whose first column is called ZONE must still go to the CSV reader.
    QStringList probeLines;
    if (decodeMonitorText(content, &probeLines, nullptr) && tecLooksLikeTecplotLines(probeLines))
        return readTecplot(content, path, out, error);
    return readCsv(content, path, out, error);
}

bool MonitorLogReader::readTecplot(const QByteArray &content, const QString &sourcePath,
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

    QStringList lines;
    if (!decodeMonitorText(content, &lines, error))
        return false;

    return parseTecplot(&log, lines, out, error);
}

bool MonitorLogReader::canReadTecplot(const QByteArray &content)
{
    QStringList lines;
    if (!decodeMonitorText(content, &lines, nullptr))
        return false;
    if (!tecLooksLikeTecplotLines(lines))
        return false;
    // The record words alone are not enough: a Tecplot file whose variables are
    // thermometer readings has nothing to plot against an iteration axis, and the
    // file dialog must not offer it.
    const QStringList names = tecVariableNames(lines);
    if (names.isEmpty())
        return false;
    return hasRecognisedColumns(columnsFromNames(names, nullptr));
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

    QStringList lines;
    if (!decodeMonitorText(content, &lines, error))
        return false;

    // --- header ---
    // The columns keep one entry per header field that carries a name, in field
    // order, so a data row is aligned by position rather than by counting. The
    // previous version dropped a repeated name from the column list and then
    // truncated every row to the shortened width, which handed each column the
    // data of its left neighbour -- a wrong number that still looked like a
    // plausible one, with only a warning that read as if it had been handled.
    QVector<int> fieldToColumn;
    log.columns = parseHeaderColumns(lines.first(), &log.warnings, &fieldToColumn);
    if (log.columns.isEmpty()) {
        if (error)
            *error = QStringLiteral("表头为空或不含可识别的列名。");
        return false;
    }

    // A generic CSV (a,b / 1,2) has no index, residual or coefficient column.
    // Accepting it would draw an arbitrary column against a row index and call
    // it a solver history, so it is refused with a readable reason instead.
    if (!hasRecognisedColumns(log.columns)) {
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
        // The admission rule lives in admitDataRow() so the tail source cannot
        // drift from it: a row too short to attribute to the columns is dropped
        // here and dropped there, with the same warning.
        QStringList fields;
        bool ragged = false;
        if (!admitDataRow(lines.at(row), columnCount, fieldToColumn, &fields, &ragged)) {
            ++raggedRows;
            ++skippedRows;
            continue;
        }
        if (ragged)
            ++raggedRows;
        for (int index = 0; index < columnCount; ++index) {
            double value = 0.0;
            if (!parseNumericField(fields.at(index), &value)) {
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

    return assembleLog(&log, raw, out, error);
}

} // namespace monitor
} // namespace aerosim