#include "aerosim/machine_learning/RbfKrr.h"
#include "aerosim/machine_learning/TimeSeriesForecast.h"

#include <cmath>
#include <iostream>
#include <limits>

using aerosim::machine_learning::RbfKrr;
using aerosim::machine_learning::RbfKrrOptions;

namespace {
int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
} // namespace

int main()
{
    RbfKrr model;
    std::string error;
    double prediction = 123.0;
    check(!model.predict({0.0, 0.0}, &prediction, &error), "reject untrained prediction");
    check(prediction == 123.0 && !error.empty(), "failed prediction preserves output");

    // Four samples of the nonlinear interaction y_next = y_previous * control.
    const RbfKrr::Samples inputs{{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    const std::vector<double> targets{1, -1, -1, 1};
    RbfKrrOptions options;
    options.gamma = 0.1;
    check(model.fit(inputs, targets, options, &error), "train four nonlinear samples");
    check(model.isTrained() && error.empty(), "training succeeds without stale error");
    for (std::size_t row = 0; row < inputs.size(); ++row) {
        check(model.predict(inputs[row], &prediction, &error), "predict training sample");
        check(std::abs(prediction - targets[row]) < 1e-4, "regularized training reconstruction");
    }

    // The held-out query is absent from training and depends on both inputs.
    check(model.predict({0.5, -0.5}, &prediction, &error), "predict held-out nonlinear sample");
    check(std::abs(prediction - (-0.25)) < 0.05, "held-out nonlinear error below 0.05");
    std::cout << "Four training samples; held-out expected=-0.25, predicted=" << prediction << '\n';

    RbfKrr scaledModel;
    RbfKrr::Samples scaledInputs;
    std::vector<double> shiftedTargets;
    for (std::size_t row = 0; row < inputs.size(); ++row) {
        scaledInputs.push_back({300 + 50 * inputs[row][0], 1e5 + 1e4 * inputs[row][1], 273});
        shiftedTargets.push_back(100 + targets[row]);
    }
    check(scaledModel.fit(scaledInputs, shiftedTargets, options, &error), "fit mixed units and a constant column");
    double scaledPrediction = 0;
    check(scaledModel.predict({325, 95000, 273}, &scaledPrediction, &error), "predict mixed units");
    check(std::abs(scaledPrediction - (100 + prediction)) < 1e-10, "training standardization and target centering");

    double state = 1.0;
    double expected = state;
    for (double control : {-1.0, 1.0, -1.0}) {
        expected *= control;
        check(model.predict({state, control}, &prediction, &error), "forecast with known control");
        check(std::abs(prediction - expected) < 1e-3, "three-step synthetic forecast");
        state = prediction;
    }

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    check(!model.fit({}, {}, options, &error), "reject empty training");
    check(!model.fit({{}}, {0}, options, &error), "reject zero features");
    check(!model.fit(inputs, {1}, options, &error), "reject target count mismatch");
    check(!model.fit({{0}, {0, 1}}, {0, 1}, options, &error), "reject inconsistent dimensions");
    check(!model.fit({{nan}}, {0}, options, &error), "reject nonfinite feature");
    check(!model.fit({{0}}, {infinity}, options, &error), "reject nonfinite target");
    RbfKrrOptions invalid = options;
    invalid.gamma = 0;
    check(!model.fit(inputs, targets, invalid, &error), "reject nonpositive gamma");
    invalid.gamma = nan;
    check(!model.fit(inputs, targets, invalid, &error), "reject nonfinite gamma");
    invalid = options;
    invalid.regularization = -1;
    check(!model.fit(inputs, targets, invalid, &error), "reject nonpositive regularization");
    invalid.regularization = infinity;
    check(!model.fit(inputs, targets, invalid, &error), "reject nonfinite regularization");
    invalid = options;
    invalid.regularization = std::numeric_limits<double>::denorm_min();
    check(!model.fit({{0}, {0}}, {1, -1}, invalid, &error), "reject numerically singular kernel");
    check(model.predict({1, 1}, &prediction, &error), "failed refit retains trained model");
    check(std::abs(prediction - 1) < 1e-4 && error.empty(), "retained model stays usable");
    prediction = 123;
    check(!model.predict({1}, &prediction, &error), "reject wrong prediction dimensions");
    check(!model.predict({nan, 0}, &prediction, &error), "reject nonfinite prediction");
    check(!model.predict({0, 0}, nullptr, &error), "reject missing output");
    check(prediction == 123, "invalid inputs preserve output");

    check(model.fit({{5, 2}, {5, 2}}, {7, 9}, options, &error), "fit duplicate and constant features");
    check(model.predict({5, 2}, &prediction, &error), "predict constant features");
    check(std::abs(prediction - 8) < 1e-8, "duplicate observations predict their mean");
    check(model.fit({{5}}, {42}, options, &error), "refit a single sample with new dimensions");
    check(model.predict({5}, &prediction, &error) && prediction == 42, "single sample prediction");

    using namespace aerosim::machine_learning;
    TimeSeriesData series;
    for (int row = 0; row < 40; ++row) {
        series.coordinates.push_back(row < 10 ? row : row + 90);
        const double value = row % 2 ? -1.0 : 1.0;
        series.inputs.push_back({value, 300});
        series.targets.push_back(value);
    }
    ForecastOptions forecasting;
    forecasting.horizon = 3;
    auto forecast = forecastRbfKrr(series, forecasting);
    check(forecast.error.empty() && forecast.segmentSamples == 30, "forecast uses the last continuous segment");
    check(forecast.future.size() == 3 && forecast.validation.size() == 3, "future and holdout sizes");
    if (forecast.future.size() == 3) {
        check(forecast.future.front().coordinate == 130, "forecast advances the selected coordinate");
        check(std::abs(forecast.future[0].prediction - 1) < 1e-4, "nonlinear window forecast");
        check(forecast.meanAbsoluteError < 1e-4, "holdout error is small");
    }
    forecasting.horizon = 20;
    const auto shortHistory = forecastRbfKrr(series, forecasting);
    check(shortHistory.error.empty() && shortHistory.future.size() == 20
          && shortHistory.validation.empty(), "short history can forecast without a holdout");
    forecasting.horizon = 40;
    check(!forecastRbfKrr(series, forecasting).error.empty(), "reject insufficient continuous history");
    forecasting.horizon = 3;
    series.inputs.back()[0] = nan;
    check(!forecastRbfKrr(series, forecasting).error.empty(), "reject a missing final input");
    series.inputs.back()[0] = -1;
    series.coordinates.back() = series.coordinates[series.coordinates.size() - 2];
    check(!forecastRbfKrr(series, forecasting).error.empty(), "reject a duplicate final coordinate");
    std::atomic_bool cancelled{true};
    check(forecastRbfKrr(series, forecasting, &cancelled).cancelled, "forecast can be cancelled");
    if (failures)
        return 1;
    std::cout << "RBF-KRR checks passed\n";
    return 0;
}
