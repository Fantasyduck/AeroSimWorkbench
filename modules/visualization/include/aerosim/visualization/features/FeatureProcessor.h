#pragma once

#include <vtkSmartPointer.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

class vtkDataSet;
class vtkPolyData;

namespace aerosim {
namespace visualization {

// Module-local API, not a replacement for contracts/ dataset or scene references.
enum class FeatureError {
    None, InvalidInput, InvalidParameter, MissingField, NonFiniteData,
    UnsupportedGeometry, DegenerateField, EmptyResult, ProcessingFailed,
    UnsupportedFormat, FileExists, IoError, Cancelled,
    NoSeeds, SeedOutsideDomain
};

struct FeatureStatus {
    FeatureError code = FeatureError::None;
    std::string message;
    bool ok() const { return code == FeatureError::None; }
};

enum class FieldAssociation { Points, Cells };
enum class CellInterpolation { Reject, AverageToPoints };

struct ScalarSelection {
    std::string name;
    FieldAssociation association = FieldAssociation::Points;
    CellInterpolation cellInterpolation = CellInterpolation::Reject;
};

struct SliceParameters {
    std::array<double, 3> origin{{0.0, 0.0, 0.0}};
    std::array<double, 3> normal{{0.0, 0.0, 1.0}};
};

// Owns a private deep copy. Consumers obtain independent copies, never writable
// access to the result held by another feature or a worker.
class FeatureGeometry {
public:
    ~FeatureGeometry();
    vtkSmartPointer<vtkPolyData> copyData() const;
    long long pointCount() const;
    long long cellCount() const;

private:
    explicit FeatureGeometry(vtkPolyData *data);
    vtkSmartPointer<vtkPolyData> data_;
    friend class FeatureProcessor;
};

struct FeatureResult {
    FeatureStatus status;
    std::shared_ptr<const FeatureGeometry> geometry;
    bool ok() const { return status.ok() && geometry != nullptr; }
};

// Stateless synchronous kernels. Execute in a worker for expensive input.
// Caller must not modify input while a call/snapshot is being made. No widget,
// renderer, global cache, file reader or scene ownership is introduced here.
class FeatureProcessor {
public:
    static FeatureResult slice(vtkDataSet *input, const SliceParameters &parameters);
    static std::vector<FeatureResult> slices(vtkDataSet *input,
                                           const std::vector<SliceParameters> &planes);
    // Input MUST be the selected 2D wall/slice, not an arbitrary volume whose
    // exterior could include far-field boundaries. Surface selection is upstream.
    static FeatureResult contourLines(vtkDataSet *surface, const ScalarSelection &field,
                                      double value);
    static FeatureResult isosurface(vtkDataSet *volume, const ScalarSelection &field,
                                   double value);
    // Accept externally generated lines/surfaces (e.g. streamlines) for export.
    static FeatureResult snapshot(vtkPolyData *geometry);

private:
    static FeatureResult result(vtkPolyData *geometry);
};

const char *featureErrorName(FeatureError error);

} // namespace visualization
} // namespace aerosim
