#pragma once

#include "FeatureProcessor.h"

#include <QString>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class vtkPolyData;

namespace aerosim {
namespace visualization {

// Names of the arrays a trajectory dataset must carry. Overridable because a
// solver may already use its own spelling; the defaults match the names VTK
// itself produces where an equivalent exists (ParticleId, ParticleAge).
//
// Unlike 7 (streamlines) a trajectory is only meaningful with a time axis, so
// time is REQUIRED here. A dataset that has lines but no time is a streamline
// result, not a trajectory, and is rejected instead of being silently shown as
// static geometry.
struct TrajectoryFieldNames {
    std::string time = "Time";                     // point, 1 component, increasing
    std::string particleId = "ParticleId";         // point, 1 component, integer
    std::string particleAge = "ParticleAge";       // optional point, 1 component
    std::string seedId = "SeedId";                 // optional point, 1 component
    std::string terminationReason = "TerminationReason"; // cell, 1 component
};

// Immutable, source-agnostic trajectory data. Both the reader (P2) and a future
// integrate step (P3) produce this same shape, so the display side never needs
// to know where a trajectory came from.
struct TrajectorySet {
    FeatureStatus status;
    // vtkPolyData with one polyline cell per trajectory, points ordered in time.
    std::shared_ptr<const FeatureGeometry> geometry;
    TrajectoryFieldNames fields;
    std::string timeUnit = "s";
    // Coordinate frame of the stored points, e.g. "body-xyz" (mesh.json uses the
    // same spelling). Empty means the caller did not state one.
    std::string coordinateFrame;
    double startTime = 0.0;
    double endTime = 0.0;
    // Per-trajectory termination reason, parallel to geometry cells and in the
    // same order. Values reuse StreamlineTermination; Unknown when the input
    // carried no termination array.
    std::vector<int> terminations;
    std::size_t trajectoryCount() const;
    bool ok() const { return status.ok() && geometry != nullptr; }
};

// Reads trajectory data written as VTK XML PolyData (.vtp) or legacy .vtk.
// Files are parsed from an in-memory string because VTK 8 filename handling is
// narrow on Windows; Unicode paths must keep working (see FeatureExporter).
class TrajectoryReader {
public:
    // Validates that the dataset really is a trajectory (required arrays above)
    // and captures counts, time range and cell-level termination reasons.
    static TrajectorySet fromPolyData(vtkPolyData *data,
                                      const TrajectoryFieldNames &fields = TrajectoryFieldNames());
    static TrajectorySet readVtp(const QString &path,
                                 const TrajectoryFieldNames &fields = TrajectoryFieldNames());
    static TrajectorySet readLegacy(const QString &path,
                                    const TrajectoryFieldNames &fields = TrajectoryFieldNames());
    // Dispatches on the suffix. Any other suffix is UnsupportedFormat rather
    // than a guess.
    static TrajectorySet read(const QString &path,
                              const TrajectoryFieldNames &fields = TrajectoryFieldNames());
};

} // namespace visualization
} // namespace aerosim
