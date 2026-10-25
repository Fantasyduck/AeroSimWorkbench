#include "aerosim/visualization/features/FeatureProcessor.h"
#include "aerosim/visualization/features/FeatureExporter.h"
#include "aerosim/visualization/features/FeatureJob.h"

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDataSetReader.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkDoubleArray.h>
#include <vtkIdList.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkVersion.h>
#include <vtkXMLPolyDataReader.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtGlobal>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace aerosim::visualization;
namespace {
void require(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}
void success(const FeatureResult &result)
{
    require(result.ok(), std::string(featureErrorName(result.status.code)) + ": " + result.status.message);
}
ScalarSelection scalar(const char *name)
{
    ScalarSelection selection;
    selection.name = name;
    return selection;
}
vtkSmartPointer<vtkImageData> analytic()
{
    auto input = vtkSmartPointer<vtkImageData>::New();
    input->SetDimensions(41, 41, 41);
    input->SetOrigin(-1, -1, -1);
    input->SetSpacing(.05, .05, .05);
    vtkNew<vtkDoubleArray> radius;
    radius->SetName("RadiusSquared");
    vtkNew<vtkDoubleArray> temperature;
    temperature->SetName("Temperature");
    for (vtkIdType i = 0; i < input->GetNumberOfPoints(); ++i) {
        double p[3];
        input->GetPoint(i, p);
        radius->InsertNextValue(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        temperature->InsertNextValue(300 + 80 * p[0] + 40 * p[1] + 20 * p[2]);
    }
    input->GetPointData()->AddArray(radius);
    input->GetPointData()->AddArray(temperature);
    return input;
}
void checkPoints(vtkPolyData *data, const std::function<bool(const double *)> &predicate)
{
    require(data->GetNumberOfPoints() > 0, "No points");
    for (vtkIdType i = 0; i < data->GetNumberOfPoints(); ++i) {
        double p[3];
        data->GetPoint(i, p);
        require(predicate(p), "Geometry coordinate mismatch");
    }
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
vtkSmartPointer<vtkPolyData> readVtp(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read export");
    const QByteArray bytes = file.readAll();
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->ReadFromInputStringOn();
    reader->SetInputString(std::string(bytes.constData(), static_cast<size_t>(bytes.size())));
    reader->Update();
    require(reader->GetErrorCode() == 0, "Invalid VTP output");
    vtkSmartPointer<vtkPolyData> output = reader->GetOutput();
    return output;
}
void compare(vtkPolyData *a, vtkPolyData *b)
{
    require(a->GetNumberOfPoints() == b->GetNumberOfPoints(), "Point count changed");
    require(a->GetNumberOfCells() == b->GetNumberOfCells(), "Cell count changed");
    for (vtkIdType i = 0; i < a->GetNumberOfPoints(); ++i) {
        double x[3], y[3];
        a->GetPoint(i, x);
        b->GetPoint(i, y);
        for (int j = 0; j < 3; ++j)
            require(std::abs(x[j] - y[j]) < 1e-12, "Coordinates changed");
    }
    vtkNew<vtkIdList> x;
    vtkNew<vtkIdList> y;
    for (vtkIdType i = 0; i < a->GetNumberOfCells(); ++i) {
        require(a->GetCellType(i) == b->GetCellType(i), "Cell type changed");
        a->GetCellPoints(i, x);
        b->GetCellPoints(i, y);
        require(x->GetNumberOfIds() == y->GetNumberOfIds(), "Cell size changed");
        for (vtkIdType j = 0; j < x->GetNumberOfIds(); ++j)
            require(x->GetId(j) == y->GetId(j), "Connectivity changed");
    }
    for (int association = 0; association < 2; ++association) {
        auto from = association ? static_cast<vtkDataSetAttributes *>(a->GetCellData()) : a->GetPointData();
        auto to = association ? static_cast<vtkDataSetAttributes *>(b->GetCellData()) : b->GetPointData();
        require(from->GetNumberOfArrays() == to->GetNumberOfArrays(), "Array count changed");
        for (int i = 0; i < from->GetNumberOfArrays(); ++i) {
            auto first = from->GetArray(i);
            auto second = to->GetArray(first->GetName());
            require(second && first->GetNumberOfComponents() == second->GetNumberOfComponents() &&
                    first->GetNumberOfTuples() == second->GetNumberOfTuples(), "Array layout changed");
            for (vtkIdType j = 0; j < first->GetNumberOfTuples(); ++j)
                for (int k = 0; k < first->GetNumberOfComponents(); ++k)
                    require(std::abs(first->GetComponent(j, k) - second->GetComponent(j, k)) < 1e-10,
                            "Array value changed");
        }
    }
}
FeatureResult finish(FeatureJob &job)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (!job.ready() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(job.ready(), "Worker timeout");
    return job.take();
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2) {
        std::cerr << "Usage: aerosim_spatial_features_test <tests/fixtures/postprocess directory>\n";
        return 2;
    }
    std::cout << "C++14 / Qt " << qVersion() << " / VTK " << vtkVersion::GetVTKVersion() << '\n';
    QTemporaryDir temp;
    if (!temp.isValid()) {
        std::cerr << "Cannot create temporary test directory\n";
        return 2;
    }
    const QString fixtures = QString::fromLocal8Bit(argv[1]);
    int failed = 0, passed = 0;
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
    test("04 multiple independent and oblique slices", [&] {
        auto input = analytic();
        SliceParameters xy, yz, oblique;
        yz.normal = {{1, 0, 0}};
        yz.origin = {{.25, 0, 0}};
        oblique.normal = {{1, 1, 1}};
        auto results = FeatureProcessor::slices(input, {xy, yz, oblique});
        for (const auto &result : results)
            success(result);
        checkPoints(results[0].geometry->copyData(), [](const double *p) { return std::abs(p[2]) < 1e-6; });
        checkPoints(results[1].geometry->copyData(), [](const double *p) { return std::abs(p[0] - .25) < 1e-6; });
        checkPoints(results[2].geometry->copyData(), [](const double *p) { return std::abs(p[0] + p[1] + p[2]) < 1e-6; });
        auto copy = results[0].geometry->copyData();
        copy->GetPointData()->RemoveArray("Temperature");
        require(results[0].geometry->copyData()->GetPointData()->HasArray("Temperature"), "Mutable result leaked");
        xy.origin[2] = .5;
        success(FeatureProcessor::slice(input, xy));
        checkPoints(results[0].geometry->copyData(), [](const double *p) { return std::abs(p[2]) < 1e-6; });
        require(input->GetPointData()->GetArray("Temperature")->GetComponent(0, 0) == 160, "Input modified");
    });
    test("05 slice contour circle and selected-surface temperature contour", [&] {
        auto input = analytic();
        auto plane = FeatureProcessor::slice(input, {});
        success(plane);
        auto circle = FeatureProcessor::contourLines(plane.geometry->copyData(), scalar("RadiusSquared"), .49);
        success(circle);
        require(circle.geometry->copyData()->GetNumberOfLines() > 0, "No lines");
        checkPoints(circle.geometry->copyData(), [](const double *p) {
            return std::abs(p[2]) < 1e-6 && std::abs(std::sqrt(p[0] * p[0] + p[1] * p[1]) - .7) < .003;
        });
        vtkNew<vtkDataSetSurfaceFilter> wall;
        wall->SetInputData(input);
        wall->Update();
        auto line = FeatureProcessor::contourLines(wall->GetOutput(), scalar("Temperature"), 300);
        success(line);
        checkPoints(line.geometry->copyData(), [](const double *p) {
            return std::abs(80 * p[0] + 40 * p[1] + 20 * p[2]) < 1e-3 &&
                std::min({std::abs(std::abs(p[0]) - 1), std::abs(std::abs(p[1]) - 1),
                          std::abs(std::abs(p[2]) - 1)}) < 1e-6;
        });
    });
    test("06 sphere isosurface", [&] {
        auto sphere = FeatureProcessor::isosurface(analytic(), scalar("RadiusSquared"), .49);
        success(sphere);
        require(sphere.geometry->copyData()->GetNumberOfPolys() > 0, "No surface polygons");
        checkPoints(sphere.geometry->copyData(), [](const double *p) {
            return std::abs(std::sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]) - .7) < .003;
        });
    });
    test("repository fixtures: three frames, four fields, Phi plane, Pressure line", [&] {
        for (int frame = 0; frame < 3; ++frame) {
            auto input = readFixture(QDir(fixtures).filePath(QStringLiteral("cube_t0%1.vtk").arg(frame)));
            require(input->GetPointData()->GetNumberOfArrays() == 4, "Not all fixture fields loaded");
            require(input->GetPointData()->GetArray("Pressure")->GetComponent(13, 0) == 100000 + 100 * frame,
                    "Wrong frame/pressure");
            auto plane = FeatureProcessor::isosurface(input, scalar("Phi"), 0);
            success(plane);
            checkPoints(plane.geometry->copyData(), [](const double *p) { return std::abs(p[0]) < 1e-6; });
            auto sliced = FeatureProcessor::slice(input, {});
            success(sliced);
            auto line = FeatureProcessor::contourLines(sliced.geometry->copyData(), scalar("Pressure"), 100000 + 100 * frame);
            success(line);
            checkPoints(line.geometry->copyData(), [](const double *p) { return std::abs(p[0]) < 1e-6 && std::abs(p[2]) < 1e-6; });
        }
    });
    test("14 VTP roundtrip: slices, lines, surfaces, coordinates, topology and fields", [&] {
        auto input = analytic();
        auto plane = FeatureProcessor::slice(input, {});
        success(plane);
        auto circle = FeatureProcessor::contourLines(plane.geometry->copyData(), scalar("RadiusSquared"), .49);
        auto sphere = FeatureProcessor::isosurface(input, scalar("RadiusSquared"), .49);
        vtkNew<vtkDataSetSurfaceFilter> outer;
        outer->SetInputData(input);
        outer->Update();
        auto wall = FeatureProcessor::contourLines(outer->GetOutput(), scalar("Temperature"), 300);
        int index = 0;
        for (const auto &result : {plane, circle, sphere, wall}) {
            success(result);
            const auto path = temp.filePath(QStringLiteral("特征-%1.vtp").arg(index++));
            auto status = FeatureExporter::writeVtp(*result.geometry, path);
            require(status.ok(), status.message);
            compare(result.geometry->copyData(), readVtp(path));
        }
        // Export API also accepts external PolyData (e.g. another module's lines).
        auto external = FeatureProcessor::snapshot(circle.geometry->copyData());
        success(external);
        auto status = FeatureExporter::writeVtp(*external.geometry, temp.filePath("external.vtp"));
        require(status.ok(), status.message);
    });
    test("14 field selection and safe failures", [&] {
        auto plane = FeatureProcessor::slice(analytic(), {});
        success(plane);
        auto path = temp.filePath("selected.vtp");
        ExportOptions options;
        options.pointArrays = {"Temperature"};
        require(FeatureExporter::writeVtp(*plane.geometry, path, options).ok(), "Selected export failed");
        auto output = readVtp(path);
        require(output->GetPointData()->GetNumberOfArrays() == 1 && output->GetPointData()->HasArray("Temperature"),
                "Wrong array selection");
        QFile file(path);
        require(file.open(QIODevice::ReadOnly), "Open export");
        auto original = file.readAll();
        file.close();
        require(FeatureExporter::writeVtp(*plane.geometry, path).code == FeatureError::FileExists, "Unexpected overwrite");
        options.overwrite = true;
        options.pointArrays = {"missing"};
        require(FeatureExporter::writeVtp(*plane.geometry, path, options).code == FeatureError::MissingField, "Missing array accepted");
        require(file.open(QIODevice::ReadOnly) && file.readAll() == original, "Failed export damaged file");
        file.close();
        require(FeatureExporter::writeVtp(*plane.geometry, temp.filePath("fake.cgns")).code == FeatureError::UnsupportedFormat,
                "Unsupported format accepted");
        require(FeatureExporter::writeVtp(*plane.geometry, temp.filePath("missing/path.vtp")).code == FeatureError::IoError,
                "Invalid path accepted");
        options.pointArrays = {"RadiusSquared"};
        require(FeatureExporter::writeVtp(*plane.geometry, path, options).ok(), "Explicit overwrite failed");
    });
    test("invalid parameters, dimensions, fields and empty/degenerate results", [&] {
        auto input = analytic();
        SliceParameters p;
        p.normal = {{0, 0, 0}};
        require(FeatureProcessor::slice(input, p).status.code == FeatureError::InvalidParameter, "Zero normal accepted");
        require(FeatureProcessor::slice(nullptr, {}).status.code == FeatureError::InvalidInput, "Null input accepted");
        p.normal = {{0, 0, 1}};
        p.origin[2] = 5;
        require(FeatureProcessor::slice(input, p).status.code == FeatureError::EmptyResult, "Outside plane accepted");
        require(FeatureProcessor::isosurface(input, scalar("missing"), 0).status.code == FeatureError::MissingField, "Unknown scalar accepted");
        require(FeatureProcessor::isosurface(input, scalar("RadiusSquared"), -1).status.code == FeatureError::EmptyResult, "Outside value accepted");
        require(FeatureProcessor::contourLines(input, scalar("Temperature"), 300).status.code == FeatureError::UnsupportedGeometry,
                "Volume silently treated as wall");
        auto plane = FeatureProcessor::slice(input, {});
        success(plane);
        require(FeatureProcessor::isosurface(plane.geometry->copyData(), scalar("Temperature"), 300).status.code == FeatureError::UnsupportedGeometry,
                "Surface used as volume");
        input->GetPointData()->GetArray("Temperature")->FillComponent(0, 300);
        require(FeatureProcessor::isosurface(input, scalar("Temperature"), 300).status.code == FeatureError::DegenerateField, "Constant field accepted");
        input->GetPointData()->GetArray("Temperature")->SetComponent(0, 0, std::numeric_limits<double>::quiet_NaN());
        require(FeatureProcessor::isosurface(input, scalar("Temperature"), 300).status.code == FeatureError::NonFiniteData, "NaN accepted");
    });
    test("explicit cell-to-point interpolation and ambiguity rejection", [&] {
        auto input = analytic();
        vtkNew<vtkDoubleArray> cells;
        cells->SetName("CellX");
        for (vtkIdType i = 0; i < input->GetNumberOfCells(); ++i)
            cells->InsertNextValue(static_cast<double>(i % 40));
        input->GetCellData()->AddArray(cells);
        auto field = scalar("CellX");
        field.association = FieldAssociation::Cells;
        require(FeatureProcessor::isosurface(input, field, 19.5).status.code == FeatureError::InvalidParameter, "Implicit cell interpolation");
        field.cellInterpolation = CellInterpolation::AverageToPoints;
        auto result = FeatureProcessor::isosurface(input, field, 19.5);
        success(result);
        checkPoints(result.geometry->copyData(), [](const double *p) { return std::abs(p[0]) < 1e-6; });
        require(!input->GetPointData()->HasArray("CellX"), "Input field association mutated");
        vtkNew<vtkDoubleArray> collision;
        collision->SetName("CellX");
        collision->SetNumberOfTuples(input->GetNumberOfPoints());
        collision->FillComponent(0, 7);
        input->GetPointData()->AddArray(collision);
        require(FeatureProcessor::isosurface(input, field, 19.5).status.code == FeatureError::InvalidParameter, "Field collision ignored");
    });
    test("worker snapshot isolation, cancellation and one-time result consumption", [&] {
        auto input = analytic();
        FeatureRequest request;
        request.operation = FeatureOperation::Isosurface;
        request.field = scalar("RadiusSquared");
        request.value = .49;
        auto job = FeatureJob::submit(input, request);
        input->GetPointData()->GetArray("RadiusSquared")->FillComponent(0, -1);
        success(finish(*job));
        require(job->take().status.code == FeatureError::InvalidParameter, "Result consumed twice");
        auto cancelled = FeatureJob::submit(analytic(), request);
        cancelled->cancel();
        require(finish(*cancelled).status.code == FeatureError::Cancelled, "Cancelled result published");
        auto bad = FeatureJob::submit(nullptr, request);
        require(finish(*bad).status.code == FeatureError::InvalidInput, "Worker swallowed validation error");
    });
    std::cout << "RESULT: " << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
