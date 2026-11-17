#pragma once

#include "aerosim/machine_learning/RbfKrr.h"

#include <atomic>

namespace aerosim {
namespace machine_learning {

struct TimeSeriesData {
    std::vector<double> coordinates;
    RbfKrr::Samples inputs;
    std::vector<double> targets;
};

struct ForecastOptions {
    int window = 2;
    int horizon = 5;
    double step = 1.0;
    RbfKrrOptions model{1e-4, 1e-8};
};

struct ForecastPoint {
    double coordinate;
    double prediction;
};

struct ForecastResult {
    std::vector<ForecastPoint> future;
    std::vector<ForecastPoint> validation;
    std::vector<double> actual;
    double meanAbsoluteError = 0;
    double baselineMeanAbsoluteError = 0;
    std::size_t segmentSamples = 0;
    std::string error;
    bool cancelled = false;
};

// Uses the last finite, regularly spaced segment. Each horizon is fitted
// directly from historical windows. When enough records are available, the
// last horizon samples are held out before refitting on the complete segment.
ForecastResult forecastRbfKrr(const TimeSeriesData &data, const ForecastOptions &options,
                             const std::atomic_bool *cancel = nullptr);

} // namespace machine_learning
} // namespace aerosim
