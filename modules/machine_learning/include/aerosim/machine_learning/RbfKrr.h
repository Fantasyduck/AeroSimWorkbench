#pragma once

#include <string>
#include <vector>

namespace aerosim {
namespace machine_learning {

struct RbfKrrOptions {
    double gamma = 1.0;
    double regularization = 1e-6;
};

// Each sample contains the same ordered input features, such as flattened
// histories of several physical quantities. Each target is a scalar response.
class RbfKrr {
public:
    using Sample = std::vector<double>;
    using Samples = std::vector<Sample>;

    // Learns feature standardization and target centering from training data.
    // Failure preserves the previous trained model.
    bool fit(const Samples &inputs, const std::vector<double> &targets,
             const RbfKrrOptions &options = RbfKrrOptions(), std::string *error = nullptr);

    // Failure preserves *value. Concurrent const predictions are supported;
    // callers must synchronize training against prediction or other training.
    bool predict(const Sample &input, double *value, std::string *error = nullptr) const;
    bool isTrained() const { return !weights_.empty(); }

private:
    Samples inputs_;
    Sample means_;
    Sample scales_;
    std::vector<double> weights_;
    double targetMean_ = 0.0;
    RbfKrrOptions options_;
};

} // namespace machine_learning
} // namespace aerosim
