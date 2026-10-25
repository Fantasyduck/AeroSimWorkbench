#pragma once

#include "FeatureProcessor.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class vtkDataSet;
class vtkPolyData;

namespace aerosim {
namespace visualization {

// Values mirror vtkStreamTracer::ReasonForTermination so they can travel through
// contracts without exposing VTK enumerations to callers.
enum class StreamlineTermination {
    OutOfDomain = 1,
    NotInitialized = 2,
    UnexpectedValue = 3,
    OutOfLength = 4,
    OutOfSteps = 5,
    Stagnation = 6,
    Unknown = -1
};

enum class SeedMode { ExplicitPoints, Line, SurfacePoints };

// Seeds are surface-local for 7 (wall streamlines). One mode is interpreted by
// the processor; the others stay at their defaults and are ignored.
struct StreamlineSeeds {
    SeedMode mode = SeedMode::ExplicitPoints;
    std::vector<std::array<double, 3>> points;      // ExplicitPoints
    std::array<double, 3> lineStart{{0.0, 0.0, 0.0}}; // Line
    std::array<double, 3> lineEnd{{0.0, 0.0, 0.0}};   // Line
    int lineResolution = 20;                        // Line
    int surfaceStride = 1;                          // SurfacePoints
};

struct StreamlineParameters {
    // Explicit field name. Never fall back to "the first vector array".
    std::string velocityField = "Velocity";
    StreamlineSeeds seeds;
    // true keeps the integration on the 2D wall (velocity projected onto the
    // cell plane). false integrates freely in 3D, which on a wall-only dataset
    // leaves the surface immediately.
    bool surfaceStreamlines = true;
    int integrationDirection = 0; // 0 forward, 1 backward, 2 both
    int integrator = 0;           // 0 RK2, 1 RK4, 2 RK45
    double maximumPropagation = 1.0e6;
    double initialStep = 0.1;
    double minimumStep = 0.01;
    double maximumStep = 1.0;
    int stepUnit = 2;             // 1 length unit, 2 cell length unit
    long long maximumSteps = 2000;
    double terminalSpeed = 1.0e-12;
};

// Every rejected line is reported, in processing order, with the rule that
// removed it. Lines are never dropped silently.
struct RejectedLine {
    enum class Reason {
        zeroLength,
        belowMinLength,
        seedOutsideDomain,
        parallelToSurfaceNormal,
        duplicate,
        overLineLimit
    };
    Reason reason = Reason::zeroLength;
    int seedIndex = -1;
    double arcLength = 0.0;
    std::string detail;
};

struct StreamlineResult {
    FeatureStatus status;
    // Surviving lines only. Cell vertex order is preserved.
    std::shared_ptr<const FeatureGeometry> geometry;
    // Parallel to geometry cells, same order. Does NOT pollute export arrays.
    std::vector<StreamlineTermination> terminations;
    std::vector<RejectedLine> rejected;
    // Lines produced by VTK before filtering; acceptedCount() <= this.
    std::size_t producedCount = 0;
    std::size_t acceptedCount() const { return terminations.size(); }
    bool ok() const { return status.ok() && geometry != nullptr; }
};

// Rule 2 and rule 6 stay off unless a caller states a physical scale: sensible
// thresholds differ by orders of magnitude between airframes and airfoils, so
// no default here would be defensible.
struct StreamlineFilter {
    double minimumArcLength = 0.0;   // 0 disables
    std::size_t maximumLines = 0;    // 0 disables
    bool dropParallelToNormal = true;
    bool deduplicateOverlapping = true;
    double dedupDistanceTolerance = 1.0e-6;
    double dedupAngleToleranceDeg = 1.0;
};

// Accepts only an explicit 2D surface (a named wall or a slice result) whose
// velocity field lives on it. A volume is rejected: its exterior would silently
// mix far-field boundaries into what the user selected.
StreamlineResult surfaceStreamlines(vtkDataSet *surface, const StreamlineParameters &parameters,
                                    const StreamlineFilter &filter = StreamlineFilter());

// Free-space variant. Requires a 3D dataset and a velocity field on its points
// or cells; no surface projection is applied.
StreamlineResult volumeStreamlines(vtkDataSet *volume, const StreamlineParameters &parameters,
                                   const StreamlineFilter &filter = StreamlineFilter());

const char *streamlineTerminationName(StreamlineTermination termination);
const char *rejectedLineReasonName(RejectedLine::Reason reason);

} // namespace visualization
} // namespace aerosim
