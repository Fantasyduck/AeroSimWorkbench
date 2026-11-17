#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <vtkSmartPointer.h>
#include <vtkDataSet.h>
#include <memory>

namespace aerosim { namespace visualization {

struct FieldInfo {
    QString name;
    QString association;
    int components = 1;
    bool hasRange = false;
    double minimum = 0;
    double maximum = 0;
};
struct RegionData {
    QString name;
    QString id;
    int dimension = 0;
    QString kind; // volume, boundary, or unclassified element section
    QVector<FieldInfo> fields;
    vtkSmartPointer<vtkDataSet> data;
};
struct ImportedData {
    QString path;
    QVector<RegionData> regions;
};
struct ImportResult {
    ImportedData dataset;
    QString error;
    bool ok() const { return error.isEmpty(); }
};

// Internal visualization API: the owner retains VTK data; no VTK arrays enter contracts.
class DataAdapter {
public:
    virtual ~DataAdapter() = default;
    virtual bool accepts(const QString &path) const = 0;
    virtual ImportResult read(const QString &path) const = 0;
};
std::unique_ptr<DataAdapter> createCgnsAdapter();

} }
