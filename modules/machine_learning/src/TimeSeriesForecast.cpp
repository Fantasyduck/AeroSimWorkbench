#include "aerosim/machine_learning/TimeSeriesForecast.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace aerosim {
namespace machine_learning {
namespace {

bool validRow(const TimeSeriesData &data, std::size_t row)
{
    return std::isfinite(data.coordinates[row]) && std::isfinite(data.targets[row])
        && std::all_of(data.inputs[row].begin(), data.inputs[row].end(),
                       [](double value) { return std::isfinite(value); });
}

void checkCancellation(const std::atomic_bool *cancel)
{
    if (cancel && cancel->load())
        throw std::runtime_error("Prediction cancelled.");
}

RbfKrr::Sample history(const TimeSeriesData &data, std::size_t last, int window)
{
    RbfKrr::Sample sample;
    for (std::size_t row = last + 1 - window; row <= last; ++row)
        sample.insert(sample.end(), data.inputs[row].begin(), data.inputs[row].end());
    return sample;
}

std::vector<double> predictFrom(const TimeSeriesData &data, std::size_t start, std::size_t end,
                               const ForecastOptions &options, const std::atomic_bool *cancel)
{
    const auto query = history(data, end - 1, options.window);
    std::vector<double> predictions;
    for (int horizon = 1; horizon <= options.horizon; ++horizon) {
        checkCancellation(cancel);
        RbfKrr::Samples inputs;
        std::vector<double> targets;
        for (std::size_t last = start + options.window - 1; last + horizon < end; ++last) {
            checkCancellation(cancel);
            inputs.push_back(history(data, last, options.window));
            targets.push_back(data.targets[last + horizon]);
        }
        RbfKrr model;
        std::string error;
        double value;
        if (!model.fit(inputs, targets, options.model, &error)
            || !model.predict(query, &value, &error))
            throw std::runtime_error(error);
        predictions.push_back(value);
    }
    return predictions;
}

} // namespace

ForecastResult forecastRbfKrr(const TimeSeriesData &data, const ForecastOptions &options,
                             const std::atomic_bool *cancel)
{
    ForecastResult result;
    try {
        checkCancellation(cancel);
        const std::size_t count = data.coordinates.size();
        if (count == 0 || data.inputs.size() != count || data.targets.size() != count
            || data.inputs.front().empty())
            throw std::runtime_error("Select matching coordinates, input columns and a target column.");
        if (options.window < 1 || options.horizon < 1
            || !std::isfinite(options.step) || options.step <= 0)
            throw std::runtime_error("Window, horizon and sampling step must be positive.");
        for (const auto &row : data.inputs) {
            if (row.size() != data.inputs.front().size())
                throw std::runtime_error("Input columns must have consistent dimensions.");
        }
        if (!validRow(data, count - 1))
            throw std::runtime_error("The last record contains missing or nonfinite selected values.");

        std::size_t start = count - 1;
        while (start > 0 && validRow(data, start - 1)) {
            checkCancellation(cancel);
            const double delta = data.coordinates[start] - data.coordinates[start - 1];
            const double tolerance = options.step * 1e-6 + 8 * std::numeric_limits<double>::epsilon()
                * std::max(std::abs(data.coordinates[start]), std::abs(data.coordinates[start - 1]));
            if (delta <= 0 || std::abs(delta - options.step) > tolerance)
                break;
            --start;
        }
        result.segmentSamples = count - start;
        const auto minimum = std::size_t(options.window) + std::size_t(options.horizon);
        if (result.segmentSamples < minimum)
            throw std::runtime_error("The latest continuous segment needs at least "
                + std::to_string(minimum) + " records for this window and prediction horizon; available "
                + std::to_string(result.segmentSamples) + ".");

        if (result.segmentSamples >= minimum + std::size_t(options.horizon)) {
            const std::size_t cutoff = count - options.horizon;
            const auto validation = predictFrom(data, start, cutoff, options, cancel);
            for (int step = 0; step < options.horizon; ++step) {
                result.validation.push_back({data.coordinates[cutoff + step], validation[step]});
                result.actual.push_back(data.targets[cutoff + step]);
                result.meanAbsoluteError += std::abs(validation[step] - data.targets[cutoff + step]) / options.horizon;
                result.baselineMeanAbsoluteError += std::abs(data.targets[cutoff - 1] - data.targets[cutoff + step])
                    / options.horizon;
            }
            if (!std::isfinite(result.meanAbsoluteError) || !std::isfinite(result.baselineMeanAbsoluteError))
                throw std::runtime_error("Validation error exceeds the numerical range.");
        }
        const auto future = predictFrom(data, start, count, options, cancel);
        for (int step = 0; step < options.horizon; ++step) {
            const double coordinate = data.coordinates.back() + (step + 1.0) * options.step;
            const double previous = result.future.empty() ? data.coordinates.back() : result.future.back().coordinate;
            if (!std::isfinite(coordinate) || coordinate <= previous)
                throw std::runtime_error("Future coordinates exceed the numerical range or resolution.");
            result.future.push_back({coordinate, future[step]});
        }
        checkCancellation(cancel);
    } catch (const std::exception &error) {
        result.future.clear();
        result.validation.clear();
        result.actual.clear();
        result.cancelled = cancel && cancel->load();
        result.error = error.what();
    }
    return result;
}

} // namespace machine_learning
} // namespace aerosim
