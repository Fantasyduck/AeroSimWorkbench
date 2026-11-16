#include "aerosim/ui/intelligent/CsvResultReader.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTextStream>

namespace aerosim {
namespace {

bool readRecord(QTextStream &input, QStringList *fields, QString *error)
{
    fields->clear();
    QString field;
    bool quoted = false;
    bool closed = false;
    while (!input.atEnd()) {
        const QString line = input.readLine();
        if (!quoted && field.isEmpty() && fields->isEmpty() && line.trimmed().isEmpty())
            return true;
        for (int i = 0; i < line.size(); ++i) {
            const QChar character = line.at(i);
            if (quoted) {
                if (character != QLatin1Char('"')) {
                    field += character;
                } else if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                    field += character;
                    ++i;
                } else {
                    quoted = false;
                    closed = true;
                }
            } else if (character == QLatin1Char(',')) {
                fields->append(closed ? field : field.trimmed());
                field.clear();
                closed = false;
            } else if (closed) {
                if (!character.isSpace()) {
                    *error = QStringLiteral(u"引号结束后包含无效字符。");
                    return false;
                }
            } else if (character == QLatin1Char('"')) {
                if (!field.trimmed().isEmpty()) {
                    *error = QStringLiteral(u"未加引号的字段中包含引号。");
                    return false;
                }
                field.clear();
                quoted = true;
            } else {
                field += character;
            }
        }
        if (!quoted) {
            fields->append(closed ? field : field.trimmed());
            return true;
        }
        field += QLatin1Char('\n');
    }
    *error = QStringLiteral(u"字段的引号未闭合。");
    return false;
}

} // namespace

bool CsvResultReader::read(const QString &path, CsvResultTable *table, QString *error)
{
    const auto fail = [error, &path](const QString &message) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), message);
        return false;
    };
    if (!table)
        return fail(QStringLiteral(u"未提供结果表。"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return fail(file.errorString());
    QTextStream input(&file);
    input.setCodec("UTF-8");
    CsvResultTable loaded;
    loaded.sourcePath = QFileInfo(path).absoluteFilePath();
    QString message;
    int record = 0;
    while (!input.atEnd()) {
        QStringList fields;
        ++record;
        if (!readRecord(input, &fields, &message))
            return fail(QStringLiteral(u"第 %1 条记录：%2").arg(record).arg(message));
        if (fields.isEmpty())
            continue;
        if (loaded.columns.isEmpty()) {
            QSet<QString> names;
            for (QString &field : fields) {
                field = field.trimmed();
                const QString key = field.toCaseFolded();
                if (key.isEmpty() || names.contains(key))
                    return fail(QStringLiteral(u"表头列名为空或重复：%1").arg(field));
                names.insert(key);
            }
            loaded.columns = fields;
            continue;
        }
        if (fields.size() > loaded.columns.size())
            return fail(QStringLiteral(u"第 %1 条记录的字段数超过表头列数。").arg(record));
        while (fields.size() < loaded.columns.size())
            fields.append(QString());
        loaded.rows.append(fields);
    }
    if (input.status() != QTextStream::Ok || file.error() != QFileDevice::NoError)
        return fail(QStringLiteral(u"读取文件失败。"));
    if (loaded.columns.isEmpty() || loaded.rows.isEmpty())
        return fail(QStringLiteral(u"CSV 缺少表头或数据记录。"));
    *table = loaded;
    if (error)
        error->clear();
    return true;
}

} // namespace aerosim
