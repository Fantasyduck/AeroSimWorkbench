#pragma once

#include "FeatureProcessor.h"
#include "StreamlineProcessor.h"
#include "TrajectorySet.h"

#include <cstddef>
#include <string>
#include <vector>

class vtkDataSet;

namespace aerosim {
namespace visualization {

// One unsteady frame: a full dataset snapshot plus the physical time it
// represents. Frames must be supplied in strictly increasing time order, and
// every dataset must stay alive and unmodified for the duration of the call.
struct TrajectoryFrame {
    vtkDataSet *data = nullptr;
    double time = 0.0;
};

struct TrajectoryParameters {
    std::string velocityField = "Velocity";
    // Reuses StreamlineSeeds so a caller drives ⑦ and ⑧ with one seed type.
    StreamlineSeeds seeds;
    double startTime = 0.0;
    double terminationTime = 0.0;
    int integrator = 1;  // 0 RK2, 1 RK4, 2 RK45, 3 RK5; same codes as StreamlineParameters
    double terminalSpeed = 1.0e-12;
    bool computeVorticity = false;
};

// Integrates a velocity field that varies with time. Unlike the steady ⑦ path
// this needs at least two frames; a single frame has no time variation to
// integrate and VTK only warns before returning meaningless output.
//
// The step size is NOT exposed: vtkParticleTracerBase keeps IntegrationStep and
// MaximumError as protected constants with no setter, so advertising one here
// would be a promise the implementation cannot keep.
class TrajectoryProcessor {
public:
    static TrajectorySet integrate(const std::vector<TrajectoryFrame> &frames,
                                   const TrajectoryParameters &parameters);
};

} // namespace visualization
} // namespace aerosim