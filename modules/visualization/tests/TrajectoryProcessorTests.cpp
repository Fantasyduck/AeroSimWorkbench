#include "aerosim/visualization/features/TrajectoryProcessor.h"
#include "aerosim/visualization/features/TrajectorySet.h"

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkStructuredPoints.h>

#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace aerosim::visualization;

namespace {
int passed = 0;
int failed = 0;

void require(bool condition, const std::string &what)
{
    if (!condition)
        throw std::runtime_error(what);
}

void requireOk(const TrajectorySet &result, const std::string &what)
{
    require(result.ok(), what + ": " + result.status.message);
}

void requireRejected(const TrajectorySet &result, FeatureError code, const char *fragment,
                     const std::string &what)
{
    require(result.status.code == code, what + ": unexpected status " + result.status.message);
    require(result.status.message.find(fragment) != std::string::npos,
            what + ": message was \"" + result.status.message + "\"");
}

// A 3x3x3 structured cube spanning [-1, 1] in every axis, carrying a constant
// point velocity. This mirrors the repository fixture layout, so the same
// numbers work for both the synthetic and the on-disk frames.
vtkSmartPointer<vtkStructuredPoints> frame(double vx, double vy, double vz)
{
    vtkSmartPointer<vtkStructuredPoints> data = vtkSmartPointer<vtkStructuredPoints>::New();
    data->SetDimensions(3, 3, 3);
    data->SetOrigin(-1.0, -1.0, -1.0);
    data->SetSpacing(1.0, 1.0, 1.0);

    vtkNew<vtkDoubleArray> velocity;
    velocity->SetName("Velocity");
    velocity->SetNumberOfComponents(3);
    for (int i = 0; i < 27; ++i) {
        const double value[3] = {vx, vy, vz};
        velocity->InsertNextTuple(value);
    }
    data->GetPointData()->AddArray(velocity);
    data->GetPointData()->SetActiveVectors("Velocity");
    return data;
}

// The smart pointers are kept beside the frames: a TrajectoryFrame only stores a
// raw pointer, so returning bare frames would let the temporaries free the
// datasets the moment the factory expression ends.
struct Fixture {
    std::vector<vtkSmartPointer<vtkStructuredPoints> > owned;
    std::vector<TrajectoryFrame> frames;
};

Fixture threeFrames(double vx, double vy, double vz)
{
    Fixture fixture;
    for (int i = 0; i < 3; ++i) {
        vtkSmartPointer<vtkStructuredPoints> data = frame(vx, vy, vz);
        TrajectoryFrame f;
        f.data = data;
        f.time = static_cast<double>(i);
        fixture.owned.push_back(data);
        fixture.frames.push_back(f);
    }
    return fixture;
}

TrajectoryParameters parametersAt(double startTime, double terminationTime)
{
    TrajectoryParameters parameters;
    parameters.startTime = startTime;
    parameters.terminationTime = terminationTime;
    parameters.seeds.mode = SeedMode::ExplicitPoints;
    parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, -1.0, -1.0}});
    return parameters;
}

// A time-varying field V = (1 + t, 0, 0). The exact answer is
// x(t) = x0 + t + t^2/2, so the integrator can be checked against a closed form
// instead of against itself.
vtkSmartPointer<vtkStructuredPoints> acceleratingFrame(double time)
{
    vtkSmartPointer<vtkStructuredPoints> data = vtkSmartPointer<vtkStructuredPoints>::New();
    data->SetDimensions(21, 3, 3);
    data->SetOrigin(-1.0, -1.0, -1.0);
    data->SetSpacing(0.25, 1.0, 1.0);

    vtkNew<vtkDoubleArray> velocity;
    velocity->SetName("Velocity");
    velocity->SetNumberOfComponents(3);
    for (int i = 0; i < 21 * 3 * 3; ++i) {
        const double value[3] = {1.0 + time, 0.0, 0.0};
        velocity->InsertNextTuple(value);
    }
    data->GetPointData()->AddArray(velocity);
    data->GetPointData()->SetActiveVectors("Velocity");
    return data;
}

Fixture acceleratingFrames()
{
    Fixture fixture;
    const int steps = 9;
    for (int k = 0; k <= steps; ++k) {
        const double time = 2.0 * (double)k / (double)steps;
        vtkSmartPointer<vtkStructuredPoints> data = acceleratingFrame(time);
        TrajectoryFrame f;
        f.data = data;
        f.time = time;
        fixture.owned.push_back(data);
        fixture.frames.push_back(f);
    }
    return fixture;
}

void test(const char *name, const std::function<void()> &body)
{
    try {
        body();
        ++passed;
        std::cout << "[PASS] " << name << "\n";
        std::cout.flush();
    } catch (const std::exception &error) {
        ++failed;
        std::cout << "[FAIL] " << name << ": " << error.what() << "\n";
        std::cout.flush();
    }
}
} // namespace

int main()
{
    test("08 unsteady integration follows the flow and reads back as a trajectory", [&] {
        Fixture fixture = threeFrames(1.0, 0.0, 0.0);
        TrajectorySet result = TrajectoryProcessor::integrate(
            fixture.frames, parametersAt(0.0, 2.0));
        requireOk(result, "integrate");
        require(result.trajectoryCount() == 1, "one seed must give one path");
        require(std::abs(result.startTime - 0.0) < 1e-9, "start time");
        require(std::abs(result.endTime - 2.0) < 1e-9, "end time");

        // The same result has to survive the P2 reader, because the display side
        // only ever sees TrajectorySet.
        vtkSmartPointer<vtkPolyData> data = result.geometry->copyData();
        TrajectorySet read = TrajectoryReader::fromPolyData(data);
        requireOk(read, "read back");
        require(read.trajectoryCount() == 1, "read back paths");
        require(std::abs(read.startTime - result.startTime) < 1e-9, "read back start");
        require(std::abs(read.endTime - result.endTime) < 1e-9, "read back end");

        vtkDataArray *xs = data->GetPoints()->GetData();
        require(xs->GetNumberOfTuples() >= 2, "a path needs at least two samples");
        for (vtkIdType i = 1; i < xs->GetNumberOfTuples(); ++i)
            require(xs->GetComponent(i, 0) > xs->GetComponent(i - 1, 0),
                    "positions must advance along the flow");
    });

    test("08 output carries time, particle id, seed id and termination arrays", [&] {
        Fixture fixture = threeFrames(1.0, 0.0, 0.0);
        TrajectorySet result = TrajectoryProcessor::integrate(
            fixture.frames, parametersAt(0.0, 2.0));
        requireOk(result, "integrate");

        require(result.fields.time == "Time", "time array name");
        require(result.fields.particleId == "ParticleId", "particle array name");
        require(result.fields.seedId == "SeedId", "seed array name");
        require(result.fields.terminationReason == "TerminationReason", "reason array name");
        require(result.terminations.size() == result.trajectoryCount(),
                "one termination per trajectory");

        vtkSmartPointer<vtkPolyData> data = result.geometry->copyData();
        require(data->GetPointData()->HasArray("Time"), "Time array");
        require(data->GetPointData()->HasArray("ParticleId"), "ParticleId array");
        require(data->GetPointData()->HasArray("SeedId"), "SeedId array");
        require(data->GetCellData()->HasArray("TerminationReason"), "reason array");
        require(data->GetLines() != nullptr
                    && data->GetLines()->GetNumberOfCells() == data->GetNumberOfCells(),
                "every cell must be a polyline");

        // The particle id is renumbered to the compact line index, so a result
        // whose paths were dropped never advertises a gap.
        vtkIntArray *particle =
            vtkIntArray::SafeDownCast(data->GetPointData()->GetArray("ParticleId"));
        require(particle != nullptr, "ParticleId must stay integer");
        for (vtkIdType i = 0; i < particle->GetNumberOfTuples(); ++i)
            require(particle->GetValue(i) == 0, "single path keeps index zero");
    });

    test("08 a single frame is rejected, not integrated without time variation", [&] {
        Fixture fixture = threeFrames(1.0, 0.0, 0.0);
        std::vector<TrajectoryFrame> one(fixture.frames.begin(), fixture.frames.begin() + 1);
        requireRejected(TrajectoryProcessor::integrate(one, parametersAt(0.0, 0.0)),
                        FeatureError::InvalidParameter, "at least two frames",
                        "single frame");
    });

    test("08 frames must arrive in strictly increasing time order", [&] {
        Fixture fixture = threeFrames(1.0, 0.0, 0.0);
        std::swap(fixture.frames[1].time, fixture.frames[2].time);
        requireRejected(TrajectoryProcessor::integrate(fixture.frames, parametersAt(0.0, 2.0)),
                        FeatureError::InvalidInput, "strictly increase", "reversed frames");
    });

    test("08 mixing dataset classes between frames is rejected", [&] {
        Fixture fixture = threeFrames(1.0, 0.0, 0.0);
        // A real second class, kept alive by the frame itself.
        vtkSmartPointer<vtkPolyData> other = vtkSmartPointer<vtkPolyData>::New();
        fixture.frames[1].data = other;
        requireRejected(TrajectoryProcessor::integrate(fixture.frames, parametersAt(0.0, 2.0)),
                        FeatureError::InvalidParameter, "same dataset class", "mixed classes");
    });

    test("08 the time window must stay inside the supplied frames", [&] {
        Fixture fixture = threeFrames(1.0, 0.0, 0.0);
        requireRejected(TrajectoryProcessor::integrate(fixture.frames, parametersAt(0.0, 5.0)),
                        FeatureError::InvalidParameter, "must not exceed the last frame time",
                        "termination past the frames");
        requireRejected(TrajectoryProcessor::integrate(fixture.frames, parametersAt(-1.0, 2.0)),
                        FeatureError::InvalidParameter, "startTime must lie inside",
                        "start before the frames");
        requireRejected(TrajectoryProcessor::integrate(fixture.frames, parametersAt(2.0, 2.0)),
                        FeatureError::InvalidParameter, "greater than startTime",
                        "empty time window");
    });

    test("08 seeds outside the domain are reported instead of vanishing", [&] {
        Fixture fixture = threeFrames(1.0, 0.0, 0.0);
        TrajectoryParameters parameters = parametersAt(0.0, 2.0);
        parameters.seeds.points[0] = std::array<double, 3>{{5.0, 5.0, 5.0}};
        requireRejected(TrajectoryProcessor::integrate(fixture.frames, parameters),
                        FeatureError::SeedOutsideDomain, "No seed lies inside the domain",
                        "outside seed");
    });

    test("08 cell-associated velocity is rejected before the integrator sees it", [&] {
        Fixture fixture = threeFrames(1.0, 0.0, 0.0);
        fixture.frames[0].data->GetPointData()->RemoveArray("Velocity");
        vtkNew<vtkDoubleArray> cellVelocity;
        cellVelocity->SetName("Velocity");
        cellVelocity->SetNumberOfComponents(3);
        for (int i = 0; i < 8; ++i) {
            const double value[3] = {1.0, 0.0, 0.0};
            cellVelocity->InsertNextTuple(value);
        }
        fixture.frames[0].data->GetCellData()->AddArray(cellVelocity);
        requireRejected(TrajectoryProcessor::integrate(fixture.frames, parametersAt(0.0, 2.0)),
                        FeatureError::MissingField, "point-associated", "cell velocity");
    });

    test("08 invalid integrators and unsupported seeds are rejected", [&] {
        Fixture fixture = threeFrames(1.0, 0.0, 0.0);
        TrajectoryParameters parameters = parametersAt(0.0, 2.0);
        parameters.integrator = 7;
        requireRejected(TrajectoryProcessor::integrate(fixture.frames, parameters),
                        FeatureError::InvalidParameter, "Integrator must be 0", "integrator");

        TrajectoryParameters surface = parametersAt(0.0, 2.0);
        surface.seeds.mode = SeedMode::SurfacePoints;
        requireRejected(TrajectoryProcessor::integrate(fixture.frames, surface),
                        FeatureError::InvalidParameter, "SurfacePoints seeds", "surface seeds");

        TrajectoryParameters noSeeds = parametersAt(0.0, 2.0);
        noSeeds.seeds.points.clear();
        requireRejected(TrajectoryProcessor::integrate(fixture.frames, noSeeds),
                        FeatureError::NoSeeds, "No seed point", "no seeds");
    });

    test("08 integrated positions match the analytic solution of an accelerating field", [&] {
        Fixture fixture = acceleratingFrames();
        TrajectoryParameters parameters;
        parameters.startTime = 0.0;
        parameters.terminationTime = 2.0;
        parameters.seeds.mode = SeedMode::ExplicitPoints;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 0.0, 0.0}});
        TrajectorySet result = TrajectoryProcessor::integrate(fixture.frames, parameters);
        requireOk(result, "integrate");
        require(result.trajectoryCount() == 1, "one path expected");

        vtkSmartPointer<vtkPolyData> data = result.geometry->copyData();
        vtkDataArray *times = data->GetPointData()->GetArray("Time");
        require(times != nullptr, "Time array missing");
        require(data->GetNumberOfPoints() >= 2, "a path needs at least two samples");

        // V = (1 + t, 0, 0) integrates to x(t) = x0 + t + t^2/2 exactly. A run
        // that silently used a constant velocity would be off by t^2/2, which at
        // t = 2 is 2.0 - hundreds of times the tolerance below.
        for (vtkIdType i = 0; i < data->GetNumberOfPoints(); ++i) {
            double point[3];
            data->GetPoint(i, point);
            const double t = times->GetComponent(i, 0);
            const double expected = -1.0 + t + 0.5 * t * t;
            require(std::abs(point[0] - expected) < 1e-5,
                    "x(t) does not match the analytic solution");
            require(std::abs(point[1]) < 1e-9, "transverse drift in y");
            require(std::abs(point[2]) < 1e-9, "transverse drift in z");
        }
    });

    test("08 a run that reaches the end of its time window reports out of length", [&] {
        Fixture fixture = acceleratingFrames();
        TrajectoryParameters parameters;
        parameters.startTime = 0.0;
        parameters.terminationTime = 2.0;
        parameters.seeds.mode = SeedMode::ExplicitPoints;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 0.0, 0.0}});
        TrajectorySet result = TrajectoryProcessor::integrate(fixture.frames, parameters);
        requireOk(result, "integrate");
        require(result.terminations.size() == 1, "one termination expected");
        require(result.terminations[0]
                    == static_cast<int>(StreamlineTermination::OutOfLength),
                "a completed run must be reported as out of length");
        require(std::abs(result.endTime - 2.0) < 1e-9, "end time must be the window end");
    });

    test("08 every integrator code integrates an accelerating field correctly", [&] {
        // The particle tracer drives its own solver through a different code path
        // than the streamline kernel, so each code is checked here as well.
        for (int code = 0; code < 4; ++code) {
            Fixture fixture = acceleratingFrames();
            TrajectoryParameters parameters;
            parameters.startTime = 0.0;
            parameters.terminationTime = 2.0;
            parameters.integrator = code;
            parameters.seeds.mode = SeedMode::ExplicitPoints;
            parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 0.0, 0.0}});
            TrajectorySet result = TrajectoryProcessor::integrate(fixture.frames, parameters);
            requireOk(result, "integrate with integrator " + std::to_string(code));
            require(result.trajectoryCount() == 1, "one path expected");

            vtkSmartPointer<vtkPolyData> data = result.geometry->copyData();
            vtkDataArray *times = data->GetPointData()->GetArray("Time");
            require(times != nullptr, "Time array missing");
            require(data->GetNumberOfPoints() >= 2, "a path needs at least two samples");
            for (vtkIdType i = 0; i < data->GetNumberOfPoints(); ++i) {
                double point[3];
                data->GetPoint(i, point);
                const double t = times->GetComponent(i, 0);
                require(std::abs(point[0] - (-1.0 + t + 0.5 * t * t)) < 1e-5,
                        "x(t) is wrong for integrator " + std::to_string(code));
            }
        }
    });

    test("08 an out-of-range integrator is rejected with a helpful message", [&] {
        Fixture fixture = threeFrames(1.0, 0.0, 0.0);
        TrajectoryParameters parameters = parametersAt(0.0, 2.0);
        parameters.integrator = 4;
        TrajectorySet result = TrajectoryProcessor::integrate(fixture.frames, parameters);
        require(!result.ok(), "code 4 must be rejected");
        require(result.status.code == FeatureError::InvalidParameter,
                "wrong error code for an out-of-range integrator");
        require(result.status.message.find("RK5") != std::string::npos,
                "the message must list the supported integrators");
    });

    std::cout << "RESULT: " << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}