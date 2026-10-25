#pragma once

#include "FeatureProcessor.h"
#include <atomic>
#include <future>
#include <memory>

namespace aerosim {
namespace visualization {

enum class FeatureOperation { Slice, ContourLines, Isosurface };

struct FeatureRequest {
    FeatureOperation operation = FeatureOperation::Slice;
    SliceParameters plane;
    ScalarSelection field;
    double value = 0.0;
};

// Small non-Qt worker adapter for callers without an execution service yet.
// Snapshot occurs on submit; only the private snapshot is processed in worker.
// Cancellation discards results; it does NOT interrupt a running VTK filter.
// Destroying a job waits for its worker: cancel and poll before GUI destruction.
class FeatureJob {
public:
    static std::unique_ptr<FeatureJob> submit(vtkDataSet *input, const FeatureRequest &request);
    ~FeatureJob();
    FeatureJob(const FeatureJob &) = delete;
    FeatureJob &operator=(const FeatureJob &) = delete;
    void cancel();
    bool ready() const;
    FeatureResult take(); // exactly once; only after ready() on a GUI caller

private:
    FeatureJob();
    std::shared_ptr<std::atomic<bool>> cancelled_;
    std::future<FeatureResult> future_;
};

} // namespace visualization
} // namespace aerosim
