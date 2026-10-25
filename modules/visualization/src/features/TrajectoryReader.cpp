#include "aerosim/visualization/features/TrajectorySet.h"

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDataSetReader.h>
#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkXMLPolyDataReader.h>

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <limits>

namespace aerosim {
namespace visualization {
namespace {
TrajectorySet failure(FeatureError code, const std::string &message)
{
    TrajectorySet result;
    result.status = {code, message};
    return result;
}

// Only one polyline per trajectory; a dataset of points without lines has no
// path to animate and a triangle mesh is a surface, not a trajectory.
FeatureStatus validateTopology(vtkPolyData *data)
{
    const vtkIdType cells = data->GetNumberOfCells();
    if (cells == 0)
        return {FeatureError::InvalidInput, "A trajectory dataset needs at least one path"};
    if (!data->GetLines() || data->GetLines()->GetNumberOfCells() != cells)
        return {FeatureError::UnsupportedGeometry,
                "Trajectories must be polyline cells, not surfaces or vertices"};
    return {};
}

// Returns nullptr after filling status; a null return always means failure.
vtkDataArray *requirePointArray(vtkPolyData *data, const std::string &name,
                                bool integer, FeatureStatus &status)
{
    vtkDataArray *array = data->GetPointData()->GetArray(name.c_str());
    if (!array) {
        status = {FeatureError::MissingField, "Trajectory field not found: " + name};
        return nullptr;
    }
    if (array->GetNumberOfComponents() != 1) {
        status = {FeatureError::InvalidInput, "Trajectory field must have one component: " + name};
        return nullptr;
    }
    if (array->GetNumberOfTuples() != data->GetNumberOfPoints()) {
        status = {FeatureError::InvalidInput, "Trajectory field length does not match points: " + name};
        return nullptr;
    }
    const int type = array->GetDataType();
    const bool isInteger = type == VTK_INT || type == VTK_ID_TYPE || type == VTK_LONG
        || type == VTK_LONG_LONG || type == VTK_SHORT || type == VTK_UNSIGNED_INT;
    if (integer && !isInteger) {
        status = {FeatureError::InvalidInput, "Trajectory field must be integral: " + name};
        return nullptr;
    }
    if (!integer) {
        for (vtkIdType i = 0; i < array->GetNumberOfTuples(); ++i) {
            if (!std::isfinite(array->GetComponent(i, 0))) {
                status = {FeatureError::NonFiniteData, "Trajectory field contains NaN/Inf: " + name};
                return nullptr;
            }
        }
    }
    return array;
}

// Time must advance along every path. A constant or decreasing time would make
// animation undefined, so it is rejected rather than silently re-sorted.
FeatureStatus validateTimeOrder(vtkPolyData *data, vtkDataArray *time)
{
    vtkCellArray *lines = data->GetLines();
    lines->InitTraversal();
    for (vtkIdType npts = 0, *ids = nullptr; lines->GetNextCell(npts, ids);) {
        for (vtkIdType i = 1; i < npts; ++i) {
            const double previous = time->GetComponent(ids[i - 1], 0);
            const double current = time->GetComponent(ids[i], 0);
            if (!(current > previous)) {
                return {FeatureError::InvalidInput,
                        "Trajectory time must strictly increase along each path"};
            }
        }
    }
    return {};
}

// Bytes are read by Qt (Unicode-safe) and handed to VTK through memory, because
// VTK 8 takes narrow filenames on Windows (same reason as FeatureExporter).
bool readBytes(const QString &path, std::string &bytes, std::string &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString().toStdString();
        return false;
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        error = file.errorString().toStdString();
        return false;
    }
    bytes.assign(data.constData(), static_cast<std::size_t>(data.size()));
    return true;
}

TrajectorySet complete(vtkPolyData *data, const TrajectoryFieldNames &fields)
{
    FeatureStatus status = validateTopology(data);
    if (!status.ok())
        return failure(status.code, status.message);
    vtkDataArray *time = requirePointArray(data, fields.time, false, status);
    if (!time)
        return failure(status.code, status.message);
    if (!requirePointArray(data, fields.particleId, true, status))
        return failure(status.code, status.message);
    // Optional arrays are accepted when present but must be well formed.
    if (data->GetPointData()->HasArray(fields.particleAge.c_str())
            && !requirePointArray(data, fields.particleAge, false, status))
        return failure(status.code, status.message);
    if (data->GetPointData()->HasArray(fields.seedId.c_str())
            && !requirePointArray(data, fields.seedId, true, status))
        return failure(status.code, status.message);

    status = validateTimeOrder(data, time);
    if (!status.ok())
        return failure(status.code, status.message);

    FeatureResult snapshot = FeatureProcessor::snapshot(data);
    if (!snapshot.ok())
        return failure(snapshot.status.code, snapshot.status.message);

    TrajectorySet result;
    result.geometry = snapshot.geometry;
    result.fields = fields;
    result.startTime = std::numeric_limits<double>::max();
    result.endTime = std::numeric_limits<double>::lowest();
    for (vtkIdType i = 0; i < time->GetNumberOfTuples(); ++i) {
        const double value = time->GetComponent(i, 0);
        result.startTime = std::min(result.startTime, value);
        result.endTime = std::max(result.endTime, value);
    }

    vtkDataArray *reasons = data->GetCellData()->GetArray(fields.terminationReason.c_str());
    result.terminations.assign(static_cast<std::size_t>(data->GetNumberOfCells()), -1);
    if (reasons) {
        if (reasons->GetNumberOfComponents() != 1
                || reasons->GetNumberOfTuples() != data->GetNumberOfCells())
            return failure(FeatureError::InvalidInput,
                           "Termination reasons must be one value per path");
        for (vtkIdType i = 0; i < reasons->GetNumberOfTuples(); ++i)
            result.terminations[static_cast<std::size_t>(i)] = static_cast<int>(reasons->GetComponent(i, 0));
    }
    return result;
}
} // namespace

std::size_t TrajectorySet::trajectoryCount() const
{
    return geometry ? static_cast<std::size_t>(geometry->cellCount()) : 0u;
}

TrajectorySet TrajectoryReader::fromPolyData(vtkPolyData *data, const TrajectoryFieldNames &fields)
{
    if (!data)
        return failure(FeatureError::InvalidInput, "A trajectory dataset is required");
    return complete(data, fields);
}

TrajectorySet TrajectoryReader::readVtp(const QString &path, const TrajectoryFieldNames &fields)
{
    std::string bytes;
    std::string error;
    if (!readBytes(path, bytes, error))
        return failure(FeatureError::IoError, error);
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->ReadFromInputStringOn();
    reader->SetInputString(bytes);
    reader->Update();
    if (!reader->GetOutput() || reader->GetOutput()->GetNumberOfPoints() == 0)
        return failure(FeatureError::InvalidInput, "VTP file contains no points");
    return complete(reader->GetOutput(), fields);
}

TrajectorySet TrajectoryReader::readLegacy(const QString &path, const TrajectoryFieldNames &fields)
{
    std::string bytes;
    std::string error;
    if (!readBytes(path, bytes, error))
        return failure(FeatureError::IoError, error);
    vtkNew<vtkDataSetReader> reader;
    // Without these the reader silently drops arrays and the trajectory would
    // look malformed for the wrong reason (docs/PostProcessTestData.md:17).
    reader->ReadAllScalarsOn();
    reader->ReadAllVectorsOn();
    reader->ReadAllFieldsOn();
    reader->ReadFromInputStringOn();
    reader->SetInputString(bytes.c_str(), static_cast<int>(bytes.size()));
    reader->Update();
    vtkPolyData *output = vtkPolyData::SafeDownCast(reader->GetOutput());
    if (!output)
        return failure(FeatureError::UnsupportedGeometry, "Legacy file is not polydata");
    return complete(output, fields);
}

TrajectorySet TrajectoryReader::read(const QString &path, const TrajectoryFieldNames &fields)
{
    if (path.isEmpty())
        return failure(FeatureError::InvalidInput, "A trajectory path is required");
    const QString suffix = QFileInfo(path).suffix();
    if (suffix.compare(QStringLiteral("vtp"), Qt::CaseInsensitive) == 0)
        return readVtp(path, fields);
    if (suffix.compare(QStringLiteral("vtk"), Qt::CaseInsensitive) == 0)
        return readLegacy(path, fields);
    return failure(FeatureError::UnsupportedFormat,
                   "Only .vtp and .vtk trajectories are supported");
}

} // namespace visualization
} // namespace aerosim
