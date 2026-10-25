#ifdef AEROSIM_FEATURES_REQUIRES_DEBUG
#error "Use Debug with the project VTK 8.2 installation"
#endif

#include "aerosim/visualization/features/FeatureProcessor.h"

#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkCellDataToPointData.h>
#include <vtkContourFilter.h>
#include <vtkCutter.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkNew.h>
#include <vtkPlane.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

namespace aerosim {
namespace visualization {
namespace {
FeatureResult failure(FeatureError code, const std::string &message)
{
    FeatureResult result;
    result.status = {code, message};
    return result;
}

FeatureStatus validate(vtkDataSet *input, int dimension = -1)
{
    if (!input || input->GetNumberOfPoints() == 0 || input->GetNumberOfCells() == 0)
        return {FeatureError::InvalidInput, "A nonempty dataset is required"};
    for (vtkIdType i = 0; i < input->GetNumberOfPoints(); ++i) {
        double point[3];
        input->GetPoint(i, point);
        for (double coordinate : point) {
            if (!std::isfinite(coordinate))
                return {FeatureError::NonFiniteData, "Non-finite coordinate"};
        }
    }
    if (dimension >= 0) {
        for (vtkIdType i = 0; i < input->GetNumberOfCells(); ++i) {
            if (input->GetCell(i)->GetCellDimension() != dimension)
                return {FeatureError::UnsupportedGeometry,
                        dimension == 3 ? "Isosurfaces require volume cells" :
                                         "Contour lines require a selected 2D surface/slice"};
        }
    }
    return {};
}

FeatureStatus prepareScalar(vtkDataSet *input, const ScalarSelection &selection,
                            vtkSmartPointer<vtkDataSet> &prepared)
{
    if (selection.name.empty())
        return {FeatureError::InvalidParameter, "Scalar name is required"};
    auto attributes = selection.association == FieldAssociation::Points
        ? static_cast<vtkDataSetAttributes *>(input->GetPointData())
        : static_cast<vtkDataSetAttributes *>(input->GetCellData());
    auto array = attributes->GetArray(selection.name.c_str());
    if (!array)
        return {FeatureError::MissingField, "Scalar not found at the requested association"};
    const auto count = selection.association == FieldAssociation::Points
        ? input->GetNumberOfPoints() : input->GetNumberOfCells();
    if (array->GetNumberOfComponents() != 1 || array->GetNumberOfTuples() != count)
        return {FeatureError::InvalidInput, "Expected one scalar per point/cell"};
    for (vtkIdType i = 0; i < count; ++i) {
        if (!std::isfinite(array->GetComponent(i, 0)))
            return {FeatureError::NonFiniteData, "Selected scalar contains NaN/Inf"};
    }
    prepared.TakeReference(input->NewInstance());
    prepared->DeepCopy(input);
    if (selection.association == FieldAssociation::Cells) {
        if (selection.cellInterpolation != CellInterpolation::AverageToPoints)
            return {FeatureError::InvalidParameter, "Cell scalar interpolation requires explicit opt-in"};
        // Do not silently overwrite an existing point field with the same name.
        if (input->GetPointData()->HasArray(selection.name.c_str()))
            return {FeatureError::InvalidParameter, "Point/cell field name collision; normalize upstream"};
        vtkNew<vtkCellDataToPointData> interpolation;
        interpolation->SetInputData(prepared);
        interpolation->PassCellDataOn();
        interpolation->ProcessAllArraysOff();
        interpolation->AddCellDataArray(selection.name.c_str());
        interpolation->Update();
        prepared->DeepCopy(interpolation->GetOutput());
    }
    return {};
}

FeatureResult extract(vtkDataSet *input, const ScalarSelection &field, double value,
                      int dimension)
{
    auto status = validate(input, dimension);
    if (!status.ok())
        return failure(status.code, status.message);
    if (!std::isfinite(value))
        return failure(FeatureError::InvalidParameter, "Isovalue must be finite");
    vtkSmartPointer<vtkDataSet> prepared;
    status = prepareScalar(input, field, prepared);
    if (!status.ok())
        return failure(status.code, status.message);
    auto scalar = prepared->GetPointData()->GetArray(field.name.c_str());
    double range[2];
    scalar->GetRange(range);
    if (range[0] == range[1])
        return failure(FeatureError::DegenerateField, "Constant field has no unique contour");
    if (value < range[0] || value > range[1])
        return failure(FeatureError::EmptyResult, "Isovalue is outside the scalar range");
    vtkNew<vtkContourFilter> filter;
    filter->SetInputData(prepared);
    filter->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS,
                                  field.name.c_str());
    filter->SetValue(0, value);
    filter->ComputeScalarsOn();
    filter->Update();
    if (filter->GetErrorCode())
        return failure(FeatureError::ProcessingFailed, "VTK contour extraction failed");
    auto output = filter->GetOutput();
    if (dimension == 2 && output->GetNumberOfLines() == 0)
        return failure(FeatureError::EmptyResult, "No contour lines at this value");
    if (dimension == 3 && output->GetNumberOfPolys() == 0)
        return failure(FeatureError::EmptyResult, "No isosurface at this value");
    return FeatureProcessor::snapshot(output);
}
} // namespace

FeatureGeometry::FeatureGeometry(vtkPolyData *data)
    : data_(vtkSmartPointer<vtkPolyData>::New())
{
    data_->DeepCopy(data);
}
FeatureGeometry::~FeatureGeometry() = default;
vtkSmartPointer<vtkPolyData> FeatureGeometry::copyData() const
{
    auto copy = vtkSmartPointer<vtkPolyData>::New();
    copy->DeepCopy(data_);
    return copy;
}
long long FeatureGeometry::pointCount() const { return data_->GetNumberOfPoints(); }
long long FeatureGeometry::cellCount() const { return data_->GetNumberOfCells(); }

FeatureResult FeatureProcessor::result(vtkPolyData *geometry)
{
    if (!geometry || geometry->GetNumberOfPoints() == 0 || geometry->GetNumberOfCells() == 0)
        return failure(FeatureError::EmptyResult, "The operation produced no geometry");
    FeatureResult result;
    result.geometry = std::shared_ptr<const FeatureGeometry>(new FeatureGeometry(geometry));
    return result;
}

FeatureResult FeatureProcessor::snapshot(vtkPolyData *geometry)
{
    auto status = validate(geometry);
    if (!status.ok())
        return failure(status.code, status.message);
    return result(geometry);
}

FeatureResult FeatureProcessor::slice(vtkDataSet *input, const SliceParameters &parameters)
{
    auto status = validate(input, 3);
    if (!status.ok())
        return failure(status.code, status.message);
    double scale = 0.0;
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(parameters.origin[i]) || !std::isfinite(parameters.normal[i]))
            return failure(FeatureError::InvalidParameter, "Plane parameters must be finite");
        scale = std::max(scale, std::abs(parameters.normal[i]));
    }
    if (scale == 0.0)
        return failure(FeatureError::InvalidParameter, "Plane normal cannot be zero");
    double normal[3];
    for (int i = 0; i < 3; ++i)
        normal[i] = parameters.normal[i] / scale;
    vtkSmartPointer<vtkDataSet> prepared;
    prepared.TakeReference(input->NewInstance());
    prepared->DeepCopy(input);
    vtkNew<vtkPlane> plane;
    plane->SetOrigin(parameters.origin[0], parameters.origin[1], parameters.origin[2]);
    plane->SetNormal(normal);
    vtkNew<vtkCutter> cutter;
    cutter->SetInputData(prepared);
    cutter->SetCutFunction(plane);
    cutter->Update();
    if (cutter->GetErrorCode())
        return failure(FeatureError::ProcessingFailed, "VTK slicing failed");
    return result(cutter->GetOutput());
}

std::vector<FeatureResult> FeatureProcessor::slices(vtkDataSet *input,
                                                  const std::vector<SliceParameters> &planes)
{
    std::vector<FeatureResult> results;
    results.reserve(planes.size());
    for (const auto &plane : planes)
        results.push_back(slice(input, plane));
    return results;
}
FeatureResult FeatureProcessor::contourLines(vtkDataSet *surface, const ScalarSelection &field,
                                             double value)
{
    return extract(surface, field, value, 2);
}
FeatureResult FeatureProcessor::isosurface(vtkDataSet *volume, const ScalarSelection &field,
                                          double value)
{
    return extract(volume, field, value, 3);
}
const char *featureErrorName(FeatureError error)
{
    switch (error) {
    case FeatureError::None: return "None";
    case FeatureError::InvalidInput: return "InvalidInput";
    case FeatureError::InvalidParameter: return "InvalidParameter";
    case FeatureError::MissingField: return "MissingField";
    case FeatureError::NonFiniteData: return "NonFiniteData";
    case FeatureError::UnsupportedGeometry: return "UnsupportedGeometry";
    case FeatureError::DegenerateField: return "DegenerateField";
    case FeatureError::EmptyResult: return "EmptyResult";
    case FeatureError::ProcessingFailed: return "ProcessingFailed";
    case FeatureError::UnsupportedFormat: return "UnsupportedFormat";
    case FeatureError::FileExists: return "FileExists";
    case FeatureError::IoError: return "IoError";
    case FeatureError::Cancelled: return "Cancelled";
    case FeatureError::NoSeeds: return "NoSeeds";
    case FeatureError::SeedOutsideDomain: return "SeedOutsideDomain";
    }
    return "Unknown";
}
} // namespace visualization
} // namespace aerosim
