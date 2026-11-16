#include "aerosim/machine_learning/RbfKrr.h"

#include AEROSIM_EIGEN_DENSE_HEADER

#include <cmath>
#include <utility>

namespace aerosim {
namespace machine_learning {
namespace {

bool fail(std::string *error, const char *message)
{
    if (error)
        *error = message;
    return false;
}

double kernel(const RbfKrr::Sample &first, const RbfKrr::Sample &second, double gamma)
{
    double squaredDistance = 0.0;
    for (std::size_t column = 0; column < first.size(); ++column) {
        const double difference = first[column] - second[column];
        squaredDistance += difference * difference;
    }
    return std::exp(-gamma * squaredDistance);
}

} // namespace

bool RbfKrr::fit(const Samples &inputs, const std::vector<double> &targets,
                 const RbfKrrOptions &options, std::string *error)
{
    if (inputs.empty() || inputs.front().empty() || targets.size() != inputs.size())
        return fail(error, "Training requires nonempty, matching inputs and targets.");
    if (!std::isfinite(options.gamma) || options.gamma <= 0.0
        || !std::isfinite(options.regularization) || options.regularization <= 0.0)
        return fail(error, "Gamma and regularization must be finite and positive.");

    const std::size_t features = inputs.front().size();
    Eigen::MatrixXd data(inputs.size(), features);
    Eigen::VectorXd response(targets.size());
    for (std::size_t row = 0; row < inputs.size(); ++row) {
        if (inputs[row].size() != features || !std::isfinite(targets[row]))
            return fail(error, "Training samples must have equal dimensions and finite values.");
        response[row] = targets[row];
        for (std::size_t column = 0; column < features; ++column) {
            if (!std::isfinite(inputs[row][column]))
                return fail(error, "Training inputs must be finite.");
            data(row, column) = inputs[row][column];
        }
    }

    const double targetMean = response.mean();
    response.array() -= targetMean;
    Sample means(features);
    Sample scales(features);
    Samples normalized = inputs;
    for (std::size_t column = 0; column < features; ++column) {
        means[column] = data.col(column).mean();
        data.col(column).array() -= means[column];
        scales[column] = data.col(column).stableNorm() / std::sqrt(double(inputs.size()));
        if (scales[column] == 0.0)
            scales[column] = 1.0;
        if (!std::isfinite(means[column]) || !std::isfinite(scales[column]))
            return fail(error, "Training values exceed the numerical range for standardization.");
        data.col(column) /= scales[column];
        for (std::size_t row = 0; row < inputs.size(); ++row)
            normalized[row][column] = data(row, column);
    }
    if (!data.allFinite() || !response.allFinite() || !std::isfinite(targetMean))
        return fail(error, "Training values exceed the numerical range.");

    Eigen::MatrixXd gram(inputs.size(), inputs.size());
    for (std::size_t row = 0; row < inputs.size(); ++row) {
        for (std::size_t column = 0; column <= row; ++column) {
            const double value = kernel(normalized[row], normalized[column], options.gamma);
            gram(row, column) = value;
            gram(column, row) = value;
        }
    }
    gram.diagonal().array() += options.regularization;
    const Eigen::LLT<Eigen::MatrixXd> solver(gram);
    if (solver.info() != Eigen::Success)
        return fail(error, "Kernel factorization failed; increase regularization.");
    const Eigen::VectorXd coefficients = solver.solve(response);
    if (!coefficients.allFinite())
        return fail(error, "Kernel solve produced nonfinite coefficients.");

    std::vector<double> weights(coefficients.data(), coefficients.data() + coefficients.size());
    inputs_ = std::move(normalized);
    means_ = std::move(means);
    scales_ = std::move(scales);
    weights_ = std::move(weights);
    targetMean_ = targetMean;
    options_ = options;
    if (error)
        error->clear();
    return true;
}

bool RbfKrr::predict(const Sample &input, double *value, std::string *error) const
{
    if (!value || !isTrained() || input.size() != means_.size())
        return fail(error, "Prediction requires a trained model, matching features and an output.");
    Sample normalized(input.size());
    for (std::size_t column = 0; column < input.size(); ++column) {
        if (!std::isfinite(input[column]))
            return fail(error, "Prediction inputs must be finite.");
        normalized[column] = (input[column] - means_[column]) / scales_[column];
        if (!std::isfinite(normalized[column]))
            return fail(error, "Prediction inputs exceed the numerical range.");
    }
    double prediction = targetMean_;
    for (std::size_t row = 0; row < inputs_.size(); ++row)
        prediction += weights_[row] * kernel(normalized, inputs_[row], options_.gamma);
    if (!std::isfinite(prediction))
        return fail(error, "Prediction exceeds the numerical range.");
    *value = prediction;
    if (error)
        error->clear();
    return true;
}

} // namespace machine_learning
} // namespace aerosim
