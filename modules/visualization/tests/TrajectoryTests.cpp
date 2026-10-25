#include "aerosim/visualization/features/FeatureExporter.h"
#include "aerosim/visualization/features/FeatureProcessor.h"
#include "aerosim/visualization/features/TrajectorySet.h"

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataWriter.h>
#include <vtkSmartPointer.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
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
// Failure cases assert both the code and a message fragment, so a reader that
// fails for the wrong reason is not mistaken for a pass.
void requireRejected(const TrajectorySet &result, FeatureError code, const char *fragment,
                     const std::string &what)
{
    require(result.status.code == code, what + ": unexpected status " + result.status.message);
    require(result.status.message.find(fragment) != std::string::npos,
            what + ": message was \"" + result.status.message + "\"");
}

// Two paths of different length so per-path handling is exercised: path 0 spans
// t = 0..2 in three points, path 1 spans t = 0.5..1.5 in two points.
vtkSmartPointer<vtkPolyData> makeTrajectories()
{
    const double xs[5] = {0.0, 1.0, 2.0, 0.0, 1.0};
    const double ys[5] = {0.0, 0.0, 0.0, 1.0, 1.0};
    const double ts[5] = {0.0, 1.0, 2.0, 0.5, 1.5};
    const int ids[5] = {0, 0, 0, 1, 1};

    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    vtkSmartPointer<vtkDoubleArray> time = vtkSmartPointer<vtkDoubleArray>::New();
    time->SetName("Time");
    time->SetNumberOfComponents(1);
    vtkSmartPointer<vtkIntArray> particle = vtkSmartPointer<vtkIntArray>::New();
    particle->SetName("ParticleId");
    particle->SetNumberOfComponents(1);
    for (int i = 0; i < 5; ++i) {
        points->InsertNextPoint(xs[i], ys[i], 0.0);
        time->InsertNextValue(ts[i]);
        particle->InsertNextValue(ids[i]);
    }

    vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
    vtkIdType first[3] = {0, 1, 2};
    vtkIdType second[2] = {3, 4};
    lines->InsertNextCell(3, first);
    lines->InsertNextCell(2, second);

    vtkSmartPointer<vtkIntArray> reasons = vtkSmartPointer<vtkIntArray>::New();
    reasons->SetName("TerminationReason");
    reasons->SetNumberOfComponents(1);
    reasons->InsertNextValue(1); // OutOfDomain
    reasons->InsertNextValue(6); // Stagnation

    vtkSmartPointer<vtkPolyData> data = vtkSmartPointer<vtkPolyData>::New();
    data->SetPoints(points);
    data->SetLines(lines);
    data->GetPointData()->AddArray(time);
    data->GetPointData()->AddArray(particle);
    data->GetCellData()->AddArray(reasons);
    return data;
}

std::shared_ptr<const FeatureGeometry> snapshotOf(vtkPolyData *data)
{
    FeatureResult result = FeatureProcessor::snapshot(data);
    require(result.ok(), "snapshot failed: " + result.status.message);
    return result.geometry;
}

void test(const char *name, const std::function<void()> &body)
{
    try {
        body();
        ++passed;
        std::cout << "[PASS] " << name << "\n";
    } catch (const std::exception &error) {
        ++failed;
        std::cout << "[FAIL] " << name << ": " << error.what() << "\n";
    }
}

struct Fixture {
    QString dir;
    QTemporaryDir temp;
};
} // namespace

int main(int argc, char **argv)
{
    Fixture fixture;
    fixture.dir = (argc > 1) ? QString::fromLocal8Bit(argv[1]) : QString();
    if (!fixture.temp.isValid()) {
        std::cout << "RESULT: 0 passed, 1 failed\n";
        return 1;
    }

    test("08 VTP roundtrip keeps paths, time range and termination reasons", [&] {
        vtkSmartPointer<vtkPolyData> data = makeTrajectories();
        const QString path = fixture.temp.filePath(QStringLiteral("traj.vtp"));
        FeatureStatus status = FeatureExporter::writeVtp(*snapshotOf(data), path);
        require(status.ok(), "write failed: " + status.message);

        TrajectorySet read = TrajectoryReader::read(path);
        requireOk(read, "read back");
        require(read.trajectoryCount() == 2, "expected two trajectories");
        require(read.timeUnit == "s", "default time unit must be seconds");
        require(std::abs(read.startTime - 0.0) < 1e-12, "start time");
        require(std::abs(read.endTime - 2.0) < 1e-12, "end time");
        require(read.terminations.size() == 2, "terminations per path");
        require(read.terminations[0] == 1 && read.terminations[1] == 6, "termination values");
    });

    test("08 legacy .vtk read loads arrays that VTK drops by default", [&] {
        vtkSmartPointer<vtkPolyData> data = makeTrajectories();
        const QString path = fixture.temp.filePath(QStringLiteral("traj_legacy.vtk"));
        vtkNew<vtkPolyDataWriter> writer;
        writer->SetFileTypeToASCII();
        writer->SetFileName(path.toLocal8Bit().constData());
        writer->SetInputData(data);
        writer->Write();
        require(QFile::exists(path), "legacy fixture was not written");

        TrajectorySet read = TrajectoryReader::readLegacy(path);
        requireOk(read, "read legacy");
        require(read.trajectoryCount() == 2, "legacy paths");
        require(read.terminations.size() == 2 && read.terminations[0] == 1,
                "legacy cell array must survive ReadAllScalarsOn");
    });

    test("08 time and particle id are required, not defaulted", [&] {
        vtkSmartPointer<vtkPolyData> data = makeTrajectories();
        data->GetPointData()->RemoveArray("Time");
        requireRejected(TrajectoryReader::fromPolyData(data), FeatureError::MissingField,
                        "Time", "missing time");

        vtkSmartPointer<vtkPolyData> other = makeTrajectories();
        other->GetPointData()->RemoveArray("ParticleId");
        requireRejected(TrajectoryReader::fromPolyData(other), FeatureError::MissingField,
                        "ParticleId", "missing particle id");
    });

    test("08 time must strictly increase along each path", [&] {
        vtkSmartPointer<vtkPolyData> data = makeTrajectories();
        vtkDoubleArray *time = vtkDoubleArray::SafeDownCast(data->GetPointData()->GetArray("Time"));
        time->SetValue(1, 0.0); // equal to the first sample
        requireRejected(TrajectoryReader::fromPolyData(data), FeatureError::InvalidInput,
                        "strictly increase", "flat time");

        vtkSmartPointer<vtkPolyData> backwards = makeTrajectories();
        vtkDoubleArray *other = vtkDoubleArray::SafeDownCast(backwards->GetPointData()->GetArray("Time"));
        other->SetValue(2, -5.0);
        requireRejected(TrajectoryReader::fromPolyData(backwards), FeatureError::InvalidInput,
                        "strictly increase", "decreasing time");
    });

    test("08 vertices and surfaces are not trajectories", [&] {
        vtkSmartPointer<vtkPolyData> vertices = makeTrajectories();
        vtkNew<vtkCellArray> verts;
        for (vtkIdType i = 0; i < vertices->GetNumberOfPoints(); ++i)
            verts->InsertNextCell(1, &i);
        vertices->SetVerts(verts);
        vertices->SetLines(nullptr);
        requireRejected(TrajectoryReader::fromPolyData(vertices), FeatureError::UnsupportedGeometry,
                        "polyline", "vertex cloud");

        // A triangle mesh would otherwise be animated as three nonsense paths.
        vtkSmartPointer<vtkPolyData> triangles = makeTrajectories();
        vtkNew<vtkCellArray> polys;
        vtkIdType tri[3] = {0, 1, 3};
        polys->InsertNextCell(3, tri);
        triangles->SetPolys(polys);
        requireRejected(TrajectoryReader::fromPolyData(triangles), FeatureError::UnsupportedGeometry,
                        "polyline", "triangle mesh");
    });

    test("08 optional arrays are validated only when present", [&] {
        vtkSmartPointer<vtkPolyData> data = makeTrajectories();
        TrajectorySet plain = TrajectoryReader::fromPolyData(data);
        requireOk(plain, "no optional arrays");

        vtkSmartPointer<vtkDoubleArray> age = vtkSmartPointer<vtkDoubleArray>::New();
        age->SetName("ParticleAge");
        age->SetNumberOfComponents(1);
        for (int i = 0; i < 5; ++i)
            age->InsertNextValue(static_cast<double>(i) * 0.25);
        data->GetPointData()->AddArray(age);
        requireOk(TrajectoryReader::fromPolyData(data), "valid optional age");

        vtkSmartPointer<vtkPolyData> broken = makeTrajectories();
        vtkSmartPointer<vtkDoubleArray> bad = vtkSmartPointer<vtkDoubleArray>::New();
        bad->SetName("ParticleAge");
        bad->SetNumberOfComponents(1);
        for (int i = 0; i < 5; ++i)
            bad->InsertNextValue(i == 2 ? std::numeric_limits<double>::quiet_NaN() : 0.0);
        broken->GetPointData()->AddArray(bad);
        requireRejected(TrajectoryReader::fromPolyData(broken), FeatureError::NonFiniteData,
                        "ParticleAge", "NaN age");

        vtkSmartPointer<vtkPolyData> fractional = makeTrajectories();
        vtkSmartPointer<vtkDoubleArray> wrong = vtkSmartPointer<vtkDoubleArray>::New();
        wrong->SetName("ParticleId");
        wrong->SetNumberOfComponents(1);
        for (int i = 0; i < 5; ++i)
            wrong->InsertNextValue(0.5);
        fractional->GetPointData()->RemoveArray("ParticleId");
        fractional->GetPointData()->AddArray(wrong);
        requireRejected(TrajectoryReader::fromPolyData(fractional), FeatureError::InvalidInput,
                        "integral", "fractional particle id");
    });

    test("08 absent termination array means unknown, not a fabricated reason", [&] {
        vtkSmartPointer<vtkPolyData> data = makeTrajectories();
        data->GetCellData()->RemoveArray("TerminationReason");
        TrajectorySet read = TrajectoryReader::fromPolyData(data);
        requireOk(read, "no termination array");
        require(read.terminations.size() == 2, "one entry per path");
        require(read.terminations[0] == -1 && read.terminations[1] == -1,
                "absent reasons must stay Unknown (-1)");
    });

    test("08 unsupported, missing and non-polydata inputs fail explicitly", [&] {
        requireRejected(TrajectoryReader::read(fixture.temp.filePath(QStringLiteral("x.csv"))),
                        FeatureError::UnsupportedFormat, ".vtp", "unsupported suffix");
        requireRejected(TrajectoryReader::read(QString()), FeatureError::InvalidInput,
                        "path is required", "empty path");
        requireRejected(TrajectoryReader::read(fixture.temp.filePath(QStringLiteral("absent.vtp"))),
                        FeatureError::IoError, "", "missing file");
        requireRejected(TrajectoryReader::fromPolyData(nullptr), FeatureError::InvalidInput,
                        "dataset is required", "null dataset");

        // The repository mesh fixture is structured points, so it is legacily
        // readable but is not a trajectory dataset.
        if (!fixture.dir.isEmpty()) {
            const QString mesh = QDir(fixture.dir).filePath(QStringLiteral("cube_t00.vtk"));
            if (QFile::exists(mesh))
                requireRejected(TrajectoryReader::readLegacy(mesh),
                                FeatureError::UnsupportedGeometry, "not polydata",
                                "structured point mesh");
        }
    });

    test("08 reader does not modify the input dataset", [&] {
        vtkSmartPointer<vtkPolyData> data = makeTrajectories();
        const vtkIdType points = data->GetNumberOfPoints();
        const vtkIdType cells = data->GetNumberOfCells();
        requireOk(TrajectoryReader::fromPolyData(data), "read in place");
        require(data->GetNumberOfPoints() == points, "point count changed");
        require(data->GetNumberOfCells() == cells, "cell count changed");
        require(data->GetPointData()->HasArray("Time"), "Time array removed by reader");
    });

    std::cout << "RESULT: " << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
