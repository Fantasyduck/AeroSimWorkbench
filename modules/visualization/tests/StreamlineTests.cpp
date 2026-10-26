#include "aerosim/visualization/features/StreamlineProcessor.h"

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkDataSetReader.h>
#include <vtkIdList.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkVersion.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace aerosim::visualization;

namespace {

void require(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

void success(const StreamlineResult &result)
{
    require(result.ok(), std::string(featureErrorName(result.status.code)) + ": "
                             + result.status.message);
}

vtkSmartPointer<vtkDataSet> readFixture(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot open fixture: " + path.toStdString());
    const QByteArray bytes = file.readAll();
    vtkNew<vtkDataSetReader> reader;
    reader->ReadFromInputStringOn();
    reader->SetInputString(bytes.constData());
    reader->ReadAllScalarsOn();
    reader->ReadAllVectorsOn();
    reader->Update();
    require(reader->GetErrorCode() == 0, "VTK legacy reader failure");
    vtkSmartPointer<vtkDataSet> output = reader->GetOutput();
    require(output && output->GetNumberOfPoints() == 27, "Expected 27 fixture nodes");
    return output;
}

// Builds one flat wall out of the fixture's 3x3x3 structured-point grid. The
// fixture stores a uniform 3x3x3 lattice, so the y=1 face is nodes with
// j-index 2; extracting it by hand here keeps this test independent of the mesh
// import module (task 1), which does not exist yet.
vtkSmartPointer<vtkPolyData> wall(vtkDataSet *input, int axis, int fixedIndex)
{
    vtkSmartPointer<vtkPolyData> surface = vtkSmartPointer<vtkPolyData>::New();
    vtkNew<vtkPoints> points;
    vtkNew<vtkDoubleArray> velocity;
    velocity->SetName("Velocity");
    velocity->SetNumberOfComponents(3);
    vtkNew<vtkCellArray> quads;
    // Index map from the 3x3 face grid to the new point ids.
    vtkIdType map[3][3];
    for (int a = 0; a < 3; ++a) {
        for (int b = 0; b < 3; ++b) {
            int ijk[3] = {a, b, fixedIndex};
            if (axis == 1) {
                ijk[0] = a;
                ijk[1] = fixedIndex;
                ijk[2] = b;
            } else if (axis == 2) {
                ijk[0] = a;
                ijk[1] = b;
                ijk[2] = fixedIndex;
            }
            const vtkIdType source = ijk[0] + 3 * (ijk[1] + 3 * ijk[2]);
            double point[3];
            input->GetPoint(source, point);
            map[a][b] = points->InsertNextPoint(point);
            double value[3];
            input->GetPointData()->GetArray("Velocity")->GetTuple(source, value);
            velocity->InsertNextTuple(value);
        }
    }
    for (int a = 0; a < 2; ++a) {
        for (int b = 0; b < 2; ++b) {
            vtkIdType ids[4] = {map[a][b], map[a + 1][b], map[a + 1][b + 1], map[a][b + 1]};
            quads->InsertNextCell(4, ids);
        }
    }
    surface->SetPoints(points);
    surface->SetPolys(quads);
    surface->GetPointData()->AddArray(velocity);
    return surface;
}

void requireMonotonicInX(vtkPolyData *lines, double sign)
{
    require(lines->GetNumberOfLines() > 0, "No streamlines produced");
    vtkNew<vtkIdList> ids;
    for (vtkIdType cell = 0; cell < lines->GetNumberOfLines(); ++cell) {
        lines->GetCellPoints(cell, ids);
        require(ids->GetNumberOfIds() >= 2, "Streamline has fewer than two points");
        double previous[3];
        double current[3];
        lines->GetPoint(ids->GetId(0), previous);
        for (vtkIdType i = 1; i < ids->GetNumberOfIds(); ++i) {
            lines->GetPoint(ids->GetId(i), current);
            require((current[0] - previous[0]) * sign >= -1e-12,
                    "Streamline is not monotonic along x");
            for (int axis = 0; axis < 3; ++axis)
                previous[axis] = current[axis];
        }
    }
}

// A tessellated cylinder wall carrying the inviscid potential-flow velocity of the
// same cylinder. A curved wall is the interesting case: on a plane a wrong
// projection still yields a plausible straight line.
vtkSmartPointer<vtkPolyData> cylinderWall(int segments, int levels, double radius)
{
    const double pi = 3.14159265358979323846;
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    vtkNew<vtkDoubleArray> velocity;
    velocity->SetName("Velocity");
    velocity->SetNumberOfComponents(3);
    for (int j = 0; j <= levels; ++j) {
        const double z = -1.0 + 2.0 * (double)j / (double)levels;
        for (int i = 0; i < segments; ++i) {
            const double angle = 2.0 * pi * (double)i / (double)segments;
            const double x = radius * std::cos(angle);
            const double y = radius * std::sin(angle);
            points->InsertNextPoint(x, y, z);
            velocity->InsertNextTuple3(1.0 - (x * x - y * y), -2.0 * x * y, 0.0);
        }
    }
    vtkNew<vtkCellArray> quads;
    for (int j = 0; j < levels; ++j) {
        for (int i = 0; i < segments; ++i) {
            const int next = (i + 1) % segments;
            vtkIdType quad[4] = {j * segments + i, j * segments + next,
                                 (j + 1) * segments + next, (j + 1) * segments + i};
            quads->InsertNextCell(4, quad);
        }
    }
    vtkSmartPointer<vtkPolyData> surface = vtkSmartPointer<vtkPolyData>::New();
    surface->SetPoints(points);
    surface->SetPolys(quads);
    surface->GetPointData()->AddArray(velocity);
    return surface;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2) {
        std::cerr << "Usage: aerosim_streamline_test <tests/fixtures/postprocess directory>\n";
        return 2;
    }
    std::cout << "C++14 / Qt " << qVersion() << " / VTK " << vtkVersion::GetVTKVersion() << '\n';
    const QString fixtures = QString::fromLocal8Bit(argv[1]);
    int failed = 0;
    int passed = 0;
    auto test = [&](const char *name, const std::function<void()> &run) {
        try {
            run();
            ++passed;
            std::cout << "[PASS] " << name << std::endl;
        } catch (const std::exception &error) {
            ++failed;
            std::cerr << "[FAIL] " << name << ": " << error.what() << std::endl;
        }
    };

    test("07 wall streamlines stay on the surface and follow the wall velocity", [&] {
        auto input = readFixture(QDir(fixtures).filePath(QStringLiteral("cube_t00.vtk")));
        auto surface = wall(input, 1, 2);
        StreamlineParameters parameters;
        parameters.seeds.mode = SeedMode::ExplicitPoints;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 1.0, 0.0}});
        auto result = surfaceStreamlines(surface, parameters);
        success(result);
        require(result.acceptedCount() == 1, "Expected exactly one streamline");
        auto lines = result.geometry->copyData();
        require(lines->GetNumberOfLines() == 1, "Expected one polyline");
        requireMonotonicInX(lines, 1.0);
        // Projection onto the wall keeps every point on y = 1, which the 3D
        // direction of the fixture velocity (1, 0, 0) already satisfies; the
        // assertion guards the plane rather than the projection maths.
        for (vtkIdType i = 0; i < lines->GetNumberOfPoints(); ++i) {
            double point[3];
            lines->GetPoint(i, point);
            require(std::abs(point[1] - 1.0) < 1e-9, "Streamline left the wall");
        }
    });

    test("07 projection removes the normal velocity component", [&] {
        auto input = readFixture(QDir(fixtures).filePath(QStringLiteral("cube_t00.vtk")));
        auto surface = wall(input, 1, 0); // y = -1 wall
        // Overwrite the wall velocity so the normal component dominates: only a
        // projected trace can stay on the surface.
        for (vtkIdType i = 0; i < surface->GetNumberOfPoints(); ++i)
            surface->GetPointData()->GetArray("Velocity")->SetTuple3(i, 1.0, 5.0, 0.0);
        StreamlineParameters parameters;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, -1.0, 0.0}});
        auto projected = surfaceStreamlines(surface, parameters);
        success(projected);
        auto lines = projected.geometry->copyData();
        require(lines->GetNumberOfLines() == 1, "Expected one projected polyline");
        for (vtkIdType i = 0; i < lines->GetNumberOfPoints(); ++i) {
            double point[3];
            lines->GetPoint(i, point);
            require(std::abs(point[1] + 1.0) < 1e-9,
                    "Projection failed: the line left the wall plane");
        }
    });

    test("07 a velocity parallel to the wall normal produces no line", [&] {
        auto input = readFixture(QDir(fixtures).filePath(QStringLiteral("cube_t00.vtk")));
        auto surface = wall(input, 1, 2);
        for (vtkIdType i = 0; i < surface->GetNumberOfPoints(); ++i)
            surface->GetPointData()->GetArray("Velocity")->SetTuple3(i, 0.0, 1.0, 0.0);
        StreamlineParameters parameters;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 1.0, 0.0}});
        auto result = surfaceStreamlines(surface, parameters);
        require(result.status.code == FeatureError::NoSeeds
                    || result.status.code == FeatureError::EmptyResult,
                std::string("Unexpected status ") + featureErrorName(result.status.code));
        require(!result.rejected.empty(), "Rejection was silent");
    });

    test("07 seeds off the wall are reported, not snapped", [&] {
        auto input = readFixture(QDir(fixtures).filePath(QStringLiteral("cube_t00.vtk")));
        auto surface = wall(input, 1, 2);
        StreamlineParameters parameters;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 0.0, 0.0}});
        auto result = surfaceStreamlines(surface, parameters);
        require(result.status.code == FeatureError::SeedOutsideDomain, "Outside seed accepted");
        require(result.rejected.size() == 1, "Outside seed not reported");
        require(result.rejected.front().reason == RejectedLine::Reason::seedOutsideDomain,
                "Wrong rejection reason");
        require(result.rejected.front().seedIndex == 0, "Wrong seed index");
    });

    test("07 a volume input is rejected with the surface-only message", [&] {
        auto input = readFixture(QDir(fixtures).filePath(QStringLiteral("cube_t00.vtk")));
        StreamlineParameters parameters;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, -1.0, -1.0}});
        auto result = surfaceStreamlines(input, parameters);
        require(result.status.code == FeatureError::UnsupportedGeometry, "Volume accepted as wall");
        require(result.status.message.find("2D surface") != std::string::npos,
                "Message does not explain the requirement");
    });

    test("07 overlapping seeds are deduplicated and the survivor is kept", [&] {
        auto input = readFixture(QDir(fixtures).filePath(QStringLiteral("cube_t00.vtk")));
        auto surface = wall(input, 1, 2);
        StreamlineParameters parameters;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 1.0, 0.0}});
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 1.0, 0.0}});
        auto result = surfaceStreamlines(surface, parameters);
        success(result);
        require(result.acceptedCount() == 1, "Duplicate line survived");
        require(result.producedCount >= 2, "Tracer did not run for both seeds");
        bool sawDuplicate = false;
        for (const auto &entry : result.rejected) {
            if (entry.reason == RejectedLine::Reason::duplicate)
                sawDuplicate = true;
        }
        require(sawDuplicate, "Duplicate rejection not recorded");
    });

    test("07 line-count limit keeps the longest lines and reports the rest", [&] {
        auto input = readFixture(QDir(fixtures).filePath(QStringLiteral("cube_t00.vtk")));
        auto surface = wall(input, 1, 2);
        StreamlineParameters parameters;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 1.0, -1.0}});
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 1.0, 0.0}});
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 1.0, 1.0}});
        StreamlineFilter filter;
        filter.maximumLines = 1;
        auto result = surfaceStreamlines(surface, parameters, filter);
        success(result);
        require(result.acceptedCount() == 1, "Line limit not applied");
        bool sawLimit = false;
        for (const auto &entry : result.rejected) {
            if (entry.reason == RejectedLine::Reason::overLineLimit)
                sawLimit = true;
        }
        require(sawLimit, "Over-limit rejection not recorded");
    });

    test("07 invalid parameters, seeds and fields are rejected before tracing", [&] {
        auto input = readFixture(QDir(fixtures).filePath(QStringLiteral("cube_t00.vtk")));
        auto surface = wall(input, 1, 2);
        StreamlineParameters parameters;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 1.0, 0.0}});

        StreamlineParameters noSeeds = parameters;
        noSeeds.seeds.points.clear();
        require(surfaceStreamlines(surface, noSeeds).status.code == FeatureError::NoSeeds,
                "Empty explicit seeds accepted");

        StreamlineParameters badField = parameters;
        badField.velocityField = "Pressure";
        require(surfaceStreamlines(surface, badField).status.code == FeatureError::MissingField,
                "Non-vector field accepted");

        StreamlineParameters missing = parameters;
        missing.velocityField = "NoSuchField";
        require(surfaceStreamlines(surface, missing).status.code == FeatureError::MissingField,
                "Missing field accepted");

        StreamlineParameters badSteps = parameters;
        badSteps.minimumStep = 2.0;
        badSteps.maximumStep = 1.0;
        require(surfaceStreamlines(surface, badSteps).status.code == FeatureError::InvalidParameter,
                "Inverted step bounds accepted");

        StreamlineParameters badDirection = parameters;
        badDirection.integrationDirection = 7;
        require(surfaceStreamlines(surface, badDirection).status.code
                    == FeatureError::InvalidParameter,
                "Invalid direction accepted");

        require(surfaceStreamlines(nullptr, parameters).status.code == FeatureError::InvalidInput,
                "Null input accepted");

        StreamlineFilter badFilter;
        badFilter.minimumArcLength = -1.0;
        require(surfaceStreamlines(surface, parameters, badFilter).status.code
                    == FeatureError::InvalidParameter,
                "Negative arc length accepted");
    });

    test("07 the wall surface is not modified", [&] {
        auto input = readFixture(QDir(fixtures).filePath(QStringLiteral("cube_t00.vtk")));
        auto surface = wall(input, 1, 2);
        const vtkIdType points = surface->GetNumberOfPoints();
        const vtkIdType cells = surface->GetNumberOfCells();
        StreamlineParameters parameters;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, 1.0, 0.0}});
        success(surfaceStreamlines(surface, parameters));
        require(surface->GetNumberOfPoints() == points && surface->GetNumberOfCells() == cells,
                "Input geometry changed");
        require(surface->GetPointData()->GetArray("Velocity")->GetNumberOfTuples() == points,
                "Input field changed");
    });

    test("07 a curved wall keeps every point on the wall at constant z", [&] {
        const int segments = 64;
        auto surface = cylinderWall(segments, 4, 1.0);
        StreamlineParameters parameters;
        parameters.seeds.mode = SeedMode::ExplicitPoints;
        // Seeds must sit exactly on tessellation nodes: the module rejects seeds
        // that do not lie on the selected surface, which is the intended
        // behaviour, so a seed placed on the true circle between two nodes is
        // outside the polygonal wall.
        const int nodeIndices[3] = {22, 26, 30};
        for (int i = 0; i < 3; ++i) {
            const double t = 2.0 * 3.14159265358979323846 * (double)nodeIndices[i]
                             / (double)segments;
            parameters.seeds.points.push_back(
                std::array<double, 3>{{std::cos(t), std::sin(t), 0.0}});
        }
        parameters.maximumPropagation = 6.0;
        auto result = surfaceStreamlines(surface, parameters);
        success(result);
        require(result.acceptedCount() == 3, "Expected three wall streamlines");
        auto lines = result.geometry->copyData();
        require(result.terminations.size()
                    == static_cast<std::size_t>(lines->GetNumberOfLines()),
                "Expected one termination per line");

        // Points must stay within the tessellation: never outside the
        // circumscribed circle, never deeper than one chord.
        const double chord = std::cos(3.14159265358979323846 / (double)segments);
        for (vtkIdType i = 0; i < lines->GetNumberOfPoints(); ++i) {
            double point[3];
            lines->GetPoint(i, point);
            const double radius = std::sqrt(point[0] * point[0] + point[1] * point[1]);
            require(radius <= 1.0 + 1e-9, "Streamline left the cylinder wall");
            require(radius >= chord - 1e-3, "Streamline fell inside the chord");
        }

        vtkNew<vtkIdList> ids;
        for (vtkIdType cell = 0; cell < lines->GetNumberOfLines(); ++cell) {
            lines->GetCellPoints(cell, ids);
            require(ids->GetNumberOfIds() >= 2, "Line has fewer than two points");
            double first[3];
            lines->GetPoint(ids->GetId(0), first);
            for (vtkIdType i = 1; i < ids->GetNumberOfIds(); ++i) {
                double point[3];
                lines->GetPoint(ids->GetId(i), point);
                require(std::abs(point[2] - first[2]) < 1e-9,
                        "A wall streamline drifted along the cylinder axis");
            }
        }
    });

    test("07 both integration directions give one line each from a single seed", [&] {
        auto input = readFixture(QDir(fixtures).filePath(QStringLiteral("cube_t00.vtk")));
        auto surface = wall(input, 1, 2);
        StreamlineParameters parameters;
        parameters.seeds.points.push_back(std::array<double, 3>{{0.0, 1.0, 0.0}});
        parameters.integrationDirection = 2; // BOTH
        auto result = surfaceStreamlines(surface, parameters);
        success(result);
        require(result.acceptedCount() == 2,
                "BOTH must give a forward and a backward line");
        auto lines = result.geometry->copyData();
        int forward = 0;
        int backward = 0;
        vtkNew<vtkIdList> ids;
        for (vtkIdType cell = 0; cell < lines->GetNumberOfLines(); ++cell) {
            lines->GetCellPoints(cell, ids);
            double start[3];
            double finish[3];
            lines->GetPoint(ids->GetId(0), start);
            lines->GetPoint(ids->GetId(ids->GetNumberOfIds() - 1), finish);
            if (finish[0] > start[0])
                ++forward;
            else if (finish[0] < start[0])
                ++backward;
        }
        require(forward == 1 && backward == 1, "The two lines must head opposite ways");
    });

    std::cout << "RESULT: " << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}