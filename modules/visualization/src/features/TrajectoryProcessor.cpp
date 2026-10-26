#ifdef AEROSIM_FEATURES_REQUIRES_DEBUG
#error "Use Debug with the project VTK 8.2 installation"
#endif

#include "aerosim/visualization/features/TrajectoryProcessor.h"

#include <vtkAlgorithm.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellLocator.h>
#include <vtkDataArray.h>
#include <vtkDataObject.h>
#include <vtkDataSet.h>
#include <vtkDoubleArray.h>
#include <vtkGenericCell.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkIntArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkParticlePathFilter.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkStreamingDemandDrivenPipeline.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

// Presents N in-memory snapshots as one temporal pipeline source. vtkParticle-
// PathFilter drives its upstream by asking for one UPDATE_TIME_STEP at a time,
// so this class must answer REQUEST_INFORMATION with the full time list and
// REQUEST_DATA with the snapshot that belongs to the requested instant.
//
// The output port declares the concrete class of the frames (not vtkDataSet):
// the executive instantiates the declared type, and an abstract base would be
// instantiated as nothing and crash on the first copy.
class AerosimTemporalSource : public vtkAlgorithm
{
public:
    static AerosimTemporalSource *New();
    vtkTypeMacro(AerosimTemporalSource, vtkAlgorithm);

    void AddSnapshot(vtkDataSet *data, double time)
    {
        this->Snapshots.push_back(data);
        this->Times.push_back(time);
    }

    void SetConcreteType(const char *name) { this->ConcreteType = name; }

protected:
    AerosimTemporalSource()
    {
        this->SetNumberOfInputPorts(0);
        this->SetNumberOfOutputPorts(1);
    }

    int ProcessRequest(vtkInformation *request, vtkInformationVector **inInfo,
                       vtkInformationVector *outInfo) override
    {
        if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA()))
            return this->RequestData(request, inInfo, outInfo);
        if (request->Has(vtkDemandDrivenPipeline::REQUEST_INFORMATION()))
            return this->RequestInformation(request, inInfo, outInfo);
        return this->Superclass::ProcessRequest(request, inInfo, outInfo);
    }

    int FillOutputPortInformation(int, vtkInformation *info) override
    {
        info->Set(vtkDataObject::DATA_TYPE_NAME(), this->ConcreteType.c_str());
        return 1;
    }

    int RequestInformation(vtkInformation *, vtkInformationVector **,
                           vtkInformationVector *outInfo)
    {
        if (this->Snapshots.empty())
            return 0;
        vtkInformation *info = outInfo->GetInformationObject(0);
        double range[2] = {this->Times.front(), this->Times.back()};
        info->Set(vtkStreamingDemandDrivenPipeline::TIME_RANGE(), range, 2);
        // The variadic Set() form stops working past roughly twenty values, so
        // the array form is used here for any frame count.
        info->Set(vtkStreamingDemandDrivenPipeline::TIME_STEPS(), &this->Times[0],
                  static_cast<int>(this->Times.size()));
        return 1;
    }

    int RequestData(vtkInformation *, vtkInformationVector **, vtkInformationVector *outInfo)
    {
        vtkInformation *info = outInfo->GetInformationObject(0);
        vtkDataObject *output = info->Get(vtkDataObject::DATA_OBJECT());
        const double requested =
            info->Get(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP());

        std::size_t best = 0;
        double bestDistance = std::numeric_limits<double>::max();
        for (std::size_t i = 0; i < this->Times.size(); ++i) {
            const double distance = std::fabs(this->Times[i] - requested);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = i;
            }
        }
        if (bestDistance > 0.0 && bestDistance > 1.0e-9 * std::fabs(requested)) {
            // The filter only ever asks for a time it published. Anything else
            // means the two sides disagree and the result would be wrong.
            vtkErrorMacro(<< "Unexpected time request " << requested);
            return 0;
        }
        output->ShallowCopy(this->Snapshots[best]);
        output->GetInformation()->Set(vtkDataObject::DATA_TIME_STEP(), requested);
        return 1;
    }

private:
    AerosimTemporalSource(const AerosimTemporalSource &) = delete;
    void operator=(const AerosimTemporalSource &) = delete;
    std::vector<vtkSmartPointer<vtkDataSet> > Snapshots;
    std::vector<double> Times;
    std::string ConcreteType;
};
vtkStandardNewMacro(AerosimTemporalSource);

namespace aerosim {
namespace visualization {
namespace {

TrajectorySet failure(FeatureError code, const std::string &message)
{
    TrajectorySet result;
    result.status = {code, message};
    return result;
}

const char *kTimeName = "Time";
const char *kParticleIdName = "ParticleId";
const char *kSeedIdName = "SeedId";
const char *kTerminationName = "TerminationReason";

// Expands the shared seed description into concrete points. SurfacePoints has no
// meaning here, because an unsteady trace has no single surface to sample.
FeatureStatus expandSeeds(const StreamlineSeeds &seeds, std::vector<std::array<double, 3> > &points)
{
    points.clear();
    if (seeds.mode == SeedMode::SurfacePoints)
        return {FeatureError::InvalidParameter,
                "SurfacePoints seeds are not available for unsteady integration"};
    if (seeds.mode == SeedMode::ExplicitPoints) {
        points = seeds.points;
    } else {
        if (seeds.lineResolution < 2)
            return {FeatureError::InvalidParameter, "Line seed resolution must be at least two"};
        points.reserve(static_cast<std::size_t>(seeds.lineResolution));
        for (int i = 0; i < seeds.lineResolution; ++i) {
            const double t = static_cast<double>(i) / (seeds.lineResolution - 1);
            std::array<double, 3> point{};
            for (int axis = 0; axis < 3; ++axis)
                point[axis] = seeds.lineStart[axis] * (1.0 - t) + seeds.lineEnd[axis] * t;
            points.push_back(point);
        }
    }
    if (points.empty())
        return {FeatureError::NoSeeds, "No seed point was supplied"};
    for (std::size_t i = 0; i < points.size(); ++i) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(points[i][axis]))
                return {FeatureError::NonFiniteData, "Seed points must be finite"};
        }
    }
    return {};
}

// Unsteady integration only accepts point-associated velocity. vtkParticleTracerBase
// calls the one-argument SelectVectors(name), which never updates VectorsType, so
// the interpolator stays on FIELD_ASSOCIATION_POINTS (its default) and a
// cell-associated array would fail every evaluation with "Can't evaluate dataset!".
FeatureStatus resolveVelocity(vtkDataSet *data, const std::string &name)
{
    if (name.empty())
        return {FeatureError::InvalidParameter, "The velocity field name is empty"};
    vtkPointData *pointData = data->GetPointData();
    vtkDataArray *array = pointData ? pointData->GetArray(name.c_str()) : nullptr;
    if (!array) {
        vtkCellData *cellData = data->GetCellData();
        if (cellData && cellData->GetArray(name.c_str())) {
            return {FeatureError::MissingField,
                    "Velocity field must be point-associated for unsteady integration: " + name};
        }
        return {FeatureError::MissingField, "Velocity field not found on points: " + name};
    }
    if (array->GetNumberOfComponents() != 3)
        return {FeatureError::MissingField, "Velocity field must have three components: " + name};
    if (array->GetNumberOfTuples() != data->GetNumberOfPoints())
        return {FeatureError::InvalidInput,
                "Velocity field does not match the dataset size: " + name};
    for (vtkIdType i = 0; i < array->GetNumberOfTuples(); ++i) {
        double value[3];
        array->GetTuple(i, value);
        if (!std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2]))
            return {FeatureError::NonFiniteData, "Velocity field contains NaN/Inf: " + name};
    }
    return {};
}

bool insideDomain(vtkDataSet *data, vtkCellLocator *locator, const double x[3])
{
    double bounds[6];
    data->GetBounds(bounds);
    double diagonal2 = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
        const double span = bounds[2 * axis + 1] - bounds[2 * axis];
        diagonal2 += span * span;
    }
    const double tolerance = std::sqrt(diagonal2) * 1.0e-9;
    for (int axis = 0; axis < 3; ++axis) {
        if (x[axis] < bounds[2 * axis] - tolerance || x[axis] > bounds[2 * axis + 1] + tolerance)
            return false;
    }
    vtkNew<vtkGenericCell> generic;
    double pcoords[3] = {0.0, 0.0, 0.0};
    std::vector<double> weights(static_cast<std::size_t>(data->GetMaxCellSize()), 0.0);
    const vtkIdType cellId =
        locator->FindCell(const_cast<double *>(x), 0.0, generic, pcoords, weights.data());
    return cellId >= 0;
}

vtkSmartPointer<vtkPolyData> buildSeedData(const std::vector<std::array<double, 3> > &points)
{
    vtkNew<vtkPoints> seedPoints;
    seedPoints->SetDataTypeToDouble();
    seedPoints->SetNumberOfPoints(static_cast<vtkIdType>(points.size()));
    vtkNew<vtkCellArray> vertices;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const vtkIdType id = static_cast<vtkIdType>(i);
        const double x[3] = {points[i][0], points[i][1], points[i][2]};
        seedPoints->SetPoint(id, x);
        vertices->InsertNextCell(1, &id);
    }
    vtkSmartPointer<vtkPolyData> seeds = vtkSmartPointer<vtkPolyData>::New();
    seeds->SetPoints(seedPoints);
    seeds->SetVerts(vertices);
    return seeds;
}

// vtkParticleTracerBase removes a particle the moment it leaves the domain and
// never records that step, so the final stored point is one step inside. The
// only observable signal left is how close it sits to the bounding box, which is
// why an early stop that is not stagnation is reported as Unknown rather than
// invented as OutOfDomain.
StreamlineTermination classifyTermination(double lastTime, double terminationTime,
                                          const double lastPoint[3], const double bounds[6],
                                          double lastSpeed, double terminalSpeed)
{
    const double timeTolerance = 1.0e-9 * std::max(1.0, std::fabs(terminationTime));
    if (lastTime >= terminationTime - timeTolerance)
        return StreamlineTermination::OutOfLength;

    if (lastSpeed <= terminalSpeed)
        return StreamlineTermination::Stagnation;

    double diagonal2 = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
        const double span = bounds[2 * axis + 1] - bounds[2 * axis];
        diagonal2 += span * span;
    }
    const double edge = std::sqrt(diagonal2) * 1.0e-3;
    for (int axis = 0; axis < 3; ++axis) {
        if (lastPoint[axis] <= bounds[2 * axis] + edge
            || lastPoint[axis] >= bounds[2 * axis + 1] - edge)
            return StreamlineTermination::OutOfDomain;
    }
    return StreamlineTermination::Unknown;
}

} // namespace

TrajectorySet TrajectoryProcessor::integrate(const std::vector<TrajectoryFrame> &frames,
                                             const TrajectoryParameters &parameters)
{
    if (frames.size() < 2) {
        return failure(FeatureError::InvalidParameter,
                       "Unsteady integration needs at least two frames; one frame carries no "
                       "time variation");
    }
    for (std::size_t i = 0; i < frames.size(); ++i) {
        if (!frames[i].data)
            return failure(FeatureError::InvalidInput, "A trajectory frame is null");
        if (!std::isfinite(frames[i].time))
            return failure(FeatureError::NonFiniteData, "Frame times must be finite");
        if (i > 0 && !(frames[i].time > frames[i - 1].time))
            return failure(FeatureError::InvalidInput,
                           "Frame times must strictly increase; the caller controls the order");
        if (i > 0 && frames[i].data->GetClassName() != frames[0].data->GetClassName())
            return failure(FeatureError::InvalidParameter,
                           "All frames must use the same dataset class");
    }

    const double firstTime = frames.front().time;
    const double lastTime = frames.back().time;
    if (!(parameters.terminationTime > parameters.startTime))
        return failure(FeatureError::InvalidParameter,
                       "terminationTime must be greater than startTime");
    if (parameters.startTime < firstTime || parameters.startTime >= lastTime)
        return failure(FeatureError::InvalidParameter,
                       "startTime must lie inside the frame time range [first, last)");
    if (parameters.terminationTime > lastTime)
        return failure(FeatureError::InvalidParameter,
                       "terminationTime must not exceed the last frame time");
    if (parameters.integrator < 0 || parameters.integrator > 2)
        return failure(FeatureError::InvalidParameter,
                       "Integrator must be 0 (RK2), 1 (RK4) or 2 (RK45)");
    if (!(parameters.terminalSpeed >= 0.0))
        return failure(FeatureError::InvalidParameter, "terminalSpeed must not be negative");

    for (std::size_t i = 0; i < frames.size(); ++i) {
        const FeatureStatus status = resolveVelocity(frames[i].data, parameters.velocityField);
        if (!status.ok())
            return failure(status.code, "Frame " + std::to_string(i) + ": " + status.message);
    }

    std::vector<std::array<double, 3> > seedPoints;
    FeatureStatus seedStatus = expandSeeds(parameters.seeds, seedPoints);
    if (!seedStatus.ok())
        return failure(seedStatus.code, seedStatus.message);

    // The filter rejects seeds outside the domain through a debug message only,
    // so they have to be tested here or they would vanish without explanation.
    std::size_t startIndex = 0;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].time <= parameters.startTime)
            startIndex = i;
        else
            break;
    }
    vtkNew<vtkCellLocator> locator;
    locator->SetDataSet(frames[startIndex].data);
    locator->BuildLocator();

    std::vector<std::array<double, 3> > keptPoints;
    std::vector<int> keptSource;
    for (std::size_t i = 0; i < seedPoints.size(); ++i) {
        const double x[3] = {seedPoints[i][0], seedPoints[i][1], seedPoints[i][2]};
        if (!insideDomain(frames[startIndex].data, locator, x))
            continue;
        keptPoints.push_back(seedPoints[i]);
        keptSource.push_back(static_cast<int>(i));
    }
    if (keptPoints.empty())
        return failure(FeatureError::SeedOutsideDomain,
                       "No seed lies inside the domain at the start time");

    vtkSmartPointer<vtkPolyData> seedData = buildSeedData(keptPoints);

    vtkNew<AerosimTemporalSource> source;
    source->SetConcreteType(frames[0].data->GetClassName());
    for (std::size_t i = 0; i < frames.size(); ++i)
        source->AddSnapshot(frames[i].data, frames[i].time);

    vtkNew<vtkParticlePathFilter> tracer;
    tracer->SetInputConnection(0, source->GetOutputPort());
    tracer->SetInputData(1, seedData);
    tracer->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS,
                                   parameters.velocityField.c_str());
    tracer->SetStartTime(parameters.startTime);
    tracer->SetTerminationTime(parameters.terminationTime);
    tracer->SetTerminalSpeed(parameters.terminalSpeed);
    tracer->SetIntegratorType(parameters.integrator);
    tracer->SetComputeVorticity(parameters.computeVorticity ? 1 : 0);
    tracer->Update();

    vtkPolyData *raw = tracer->GetOutput();
    if (!raw || raw->GetLines() == nullptr || raw->GetLines()->GetNumberOfCells() == 0) {
        return failure(FeatureError::EmptyResult,
                       "Integration produced no path with more than one sample");
    }

    vtkDataArray *rawTime = raw->GetPointData()->GetArray("SimulationTime");
    vtkDataArray *rawParticle = raw->GetPointData()->GetArray("ParticleId");
    vtkDataArray *rawVelocity = raw->GetPointData()->GetArray(parameters.velocityField.c_str());
    if (!rawTime)
        return failure(FeatureError::ProcessingFailed,
                       "The integrator did not report SimulationTime");

    double bounds[6];
    for (int i = 0; i < 3; ++i) {
        bounds[2 * i] = std::numeric_limits<double>::max();
        bounds[2 * i + 1] = std::numeric_limits<double>::lowest();
    }
    for (std::size_t i = 0; i < frames.size(); ++i) {
        double frameBounds[6];
        frames[i].data->GetBounds(frameBounds);
        for (int axis = 0; axis < 3; ++axis) {
            bounds[2 * axis] = std::min(bounds[2 * axis], frameBounds[2 * axis]);
            bounds[2 * axis + 1] = std::max(bounds[2 * axis + 1], frameBounds[2 * axis + 1]);
        }
    }

    vtkNew<vtkPoints> outPoints;
    outPoints->SetDataTypeToDouble();
    vtkNew<vtkCellArray> outLines;
    vtkNew<vtkDoubleArray> outTime;
    outTime->SetName(kTimeName);
    vtkNew<vtkIntArray> outParticle;
    outParticle->SetName(kParticleIdName);
    vtkNew<vtkIntArray> outSeed;
    outSeed->SetName(kSeedIdName);
    vtkNew<vtkIntArray> outTermination;
    outTermination->SetName(kTerminationName);

    std::vector<int> terminations;
    double minTime = std::numeric_limits<double>::max();
    double maxTime = std::numeric_limits<double>::lowest();

    vtkIdType pointCount = 0;
    vtkIdType *pointIds = nullptr;
    int lineIndex = 0;
    raw->GetLines()->InitTraversal();
    while (raw->GetLines()->GetNextCell(pointCount, pointIds)) {
        // A path with a single sample is a particle that left the domain or
        // stalled almost immediately. It cannot be drawn and would make the
        // output non-polyline, so it is dropped here.
        if (pointCount < 2)
            continue;

        const vtkIdType firstId = pointIds[0];
        const vtkIdType lastId = pointIds[pointCount - 1];
        const int seedIndex = rawParticle
                                  ? static_cast<int>(rawParticle->GetComponent(firstId, 0))
                                  : lineIndex;
        const int sourceIndex =
            (seedIndex >= 0 && seedIndex < static_cast<int>(keptSource.size()))
                ? keptSource[static_cast<std::size_t>(seedIndex)]
                : seedIndex;

        double lastSpeed = std::numeric_limits<double>::max();
        if (rawVelocity) {
            double velocity[3];
            rawVelocity->GetTuple(lastId, velocity);
            lastSpeed = vtkMath::Norm(velocity);
        }
        double lastPoint[3];
        raw->GetPoint(lastId, lastPoint);
        const double lastSampleTime = rawTime->GetComponent(lastId, 0);

        terminations.push_back(static_cast<int>(classifyTermination(
            lastSampleTime, parameters.terminationTime, lastPoint, bounds, lastSpeed,
            parameters.terminalSpeed)));

        const vtkIdType startPoint = outPoints->GetNumberOfPoints();
        for (vtkIdType k = 0; k < pointCount; ++k) {
            double point[3];
            raw->GetPoint(pointIds[k], point);
            outPoints->InsertNextPoint(point);
            const double time = rawTime->GetComponent(pointIds[k], 0);
            outTime->InsertNextValue(time);
            outParticle->InsertNextValue(lineIndex);
            outSeed->InsertNextValue(sourceIndex);
            minTime = std::min(minTime, time);
            maxTime = std::max(maxTime, time);
        }
        outLines->InsertNextCell(pointCount);
        for (vtkIdType k = 0; k < pointCount; ++k)
            outLines->InsertCellPoint(startPoint + k);
        outTermination->InsertNextValue(terminations.back());
        ++lineIndex;
    }

    if (outLines->GetNumberOfCells() == 0)
        return failure(FeatureError::EmptyResult,
                       "Every integrated path ended at its first sample");

    vtkNew<vtkPolyData> clean;
    clean->SetPoints(outPoints);
    clean->SetLines(outLines);
    clean->GetPointData()->AddArray(outTime);
    clean->GetPointData()->AddArray(outParticle);
    clean->GetPointData()->AddArray(outSeed);
    clean->GetCellData()->AddArray(outTermination);

    FeatureResult snapshot = FeatureProcessor::snapshot(clean);
    if (!snapshot.ok())
        return failure(snapshot.status.code, snapshot.status.message);

    TrajectorySet result;
    result.geometry = snapshot.geometry;

    result.fields.time = kTimeName;
    result.fields.particleId = kParticleIdName;
    result.fields.seedId = kSeedIdName;
    result.fields.terminationReason = kTerminationName;
    result.startTime = minTime;
    result.endTime = maxTime;
    result.terminations = terminations;
    result.status = {};
    return result;
}

} // namespace visualization
} // namespace aerosim
