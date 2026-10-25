#include "aerosim/visualization/features/FeatureExporter.h"

#include <vtkAbstractArray.h>
#include <vtkCellData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkXMLPolyDataWriter.h>

#include <QFileInfo>
#include <QSaveFile>
#include <algorithm>

namespace aerosim {
namespace visualization {
namespace {
FeatureStatus selectArrays(vtkDataSetAttributes *arrays,
                           const std::vector<std::string> &selected)
{
    if (selected.empty())
        return {};
    for (const auto &name : selected) {
        if (!arrays->HasArray(name.c_str()))
            return {FeatureError::MissingField, "Export array not found: " + name};
    }
    for (int i = arrays->GetNumberOfArrays() - 1; i >= 0; --i) {
        const char *name = arrays->GetArrayName(i);
        if (!name || std::find(selected.begin(), selected.end(), name) == selected.end())
            arrays->RemoveArray(i);
    }
    return {};
}
} // namespace

FeatureStatus FeatureExporter::writeVtp(const FeatureGeometry &geometry, const QString &path,
                                        const ExportOptions &options)
{
    if (path.isEmpty() || QFileInfo(path).suffix().compare(QStringLiteral("vtp"), Qt::CaseInsensitive))
        return {FeatureError::UnsupportedFormat, "Only .vtp (VTK XML PolyData) is supported"};
    if (!options.overwrite && QFileInfo::exists(path))
        return {FeatureError::FileExists, "Output already exists"};
    auto copy = geometry.copyData();
    auto status = selectArrays(copy->GetPointData(), options.pointArrays);
    if (!status.ok())
        return status;
    status = selectArrays(copy->GetCellData(), options.cellArrays);
    if (!status.ok())
        return status;
    // ASCII XML through an in-memory string avoids VTK 8 narrow-filename Unicode
    // limitations on Windows. QSaveFile performs the Unicode-path atomic commit.
    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetInputData(copy);
    writer->SetDataModeToAscii();
    writer->SetWriteToOutputString(true);
    if (!writer->Write() || writer->GetErrorCode())
        return {FeatureError::IoError, "VTK serialization failed"};
    const std::string bytes = writer->GetOutputString();
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
        return {FeatureError::IoError, file.errorString().toStdString()};
    if (file.write(bytes.data(), static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size())) {
        file.cancelWriting();
        return {FeatureError::IoError, file.errorString().toStdString()};
    }
    if (!file.commit())
        return {FeatureError::IoError, file.errorString().toStdString()};
    return {};
}
} // namespace visualization
} // namespace aerosim
