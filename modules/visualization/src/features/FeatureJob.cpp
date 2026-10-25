#include "aerosim/visualization/features/FeatureJob.h"
#include <vtkDataSet.h>
#include <chrono>
#include <exception>

namespace aerosim {
namespace visualization {
namespace {
FeatureResult cancelledResult()
{
    FeatureResult result;
    result.status = {FeatureError::Cancelled, "Request was cancelled; result discarded"};
    return result;
}
} // namespace

FeatureJob::FeatureJob() : cancelled_(std::make_shared<std::atomic<bool>>(false)) {}
FeatureJob::~FeatureJob()
{
    cancel();
    if (future_.valid())
        future_.wait();
}
std::unique_ptr<FeatureJob> FeatureJob::submit(vtkDataSet *input, const FeatureRequest &request)
{
    std::unique_ptr<FeatureJob> job(new FeatureJob);
    vtkSmartPointer<vtkDataSet> snapshot;
    if (input) {
        snapshot.TakeReference(input->NewInstance());
        snapshot->DeepCopy(input);
    }
    auto cancelled = job->cancelled_;
    job->future_ = std::async(std::launch::async, [snapshot, request, cancelled] {
        if (cancelled->load())
            return cancelledResult();
        FeatureResult result;
        try {
            switch (request.operation) {
            case FeatureOperation::Slice:
                result = FeatureProcessor::slice(snapshot, request.plane);
                break;
            case FeatureOperation::ContourLines:
                result = FeatureProcessor::contourLines(snapshot, request.field, request.value);
                break;
            case FeatureOperation::Isosurface:
                result = FeatureProcessor::isosurface(snapshot, request.field, request.value);
                break;
            }
        } catch (const std::exception &error) {
            result.status = {FeatureError::ProcessingFailed, error.what()};
        } catch (...) {
            result.status = {FeatureError::ProcessingFailed, "Unknown worker error"};
        }
        return cancelled->load() ? cancelledResult() : result;
    });
    return job;
}
void FeatureJob::cancel() { cancelled_->store(true); }
bool FeatureJob::ready() const
{
    return future_.valid() && future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}
FeatureResult FeatureJob::take()
{
    if (!future_.valid()) {
        FeatureResult result;
        result.status = {FeatureError::InvalidParameter, "Result already consumed"};
        return result;
    }
    auto result = future_.get();
    return cancelled_->load() ? cancelledResult() : result;
}
} // namespace visualization
} // namespace aerosim
