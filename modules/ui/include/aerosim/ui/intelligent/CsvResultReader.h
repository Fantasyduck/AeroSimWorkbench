#pragma once

#include <QStringList>
#include <QVector>

namespace aerosim {

struct CsvResultTable {
    QString sourcePath;
    QStringList columns;
    QVector<QStringList> rows;
};

// Reads UTF-8 CSV records without imposing physical quantities or numeric types.
class CsvResultReader {
public:
    // A failed read leaves the caller's table untouched.
    static bool read(const QString &path, CsvResultTable *table, QString *error);
};

} // namespace aerosim
