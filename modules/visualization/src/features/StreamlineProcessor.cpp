#ifdef AEROSIM_FEATURES_REQUIRES_DEBUG
#error "Use Debug with the project VTK 8.2 installation"
#endif

#include "aerosim/visualization/features/StreamlineProcessor.h"

#include "aerosim/visualization/features/RungeKutta5.h"

#include <vtkCell.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellLocator.h>
#include <vtkDataArray.h>
#include <vtkDataObject.h>
#include <vtkDataSet.h>
#include <vtkGenericCell.h>
#include <vtkIntArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkStreamTracer.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace aerosim {
namespace visualization {
namespace {

StreamlineResult failure(FeatureError code, std::string message)
{
    StreamlineResult result;
    result.status = {code, std::move(message)};
    return result;
}

// FeatureProcessor keeps an identically shaped validator in its own anonymous
// namespace, so it cannot be shared across translation units.
FeatureStatus validateInput(vtkDataSet *input, int dimension)
{
    if (!input || input->GetNumberOfPoints() == 0 || input->GetNumberOfCells() == 0)
        return {FeatureError::InvalidInput, "A nonempty dataset is required"};
    for (vtkIdType i = 0; i < input->GetNumberOfPoints(); ++i) {
        double point[3];
        input->GetPoint(i, point);
        for (double coordinate : point) {
            if (!std::isfinite(coordinate))
                return {FeatureError::NonFiniteData, "Non-finite coordinate"};
        }
    }
    for (vtkIdType i = 0; i < input->GetNumberOfCells(); ++i) {
        if (input->GetCell(i)->GetCellDimension() != dimension) {
            return {FeatureError::UnsupportedGeometry,
                    dimension == 2
                        ? "Surface streamlines require a selected 2D surface or slice; "
                          "a volume exterior would mix in far-field boundaries"
                        : "Volume streamlines require volume cells"};
        }
    }
    return {};
}

FeatureStatus validateParameters(const StreamlineParameters &parameters)
{
    if (parameters.velocityField.empty())
        return {FeatureError::InvalidParameter, "A velocity field name is required"};
    if (parameters.integrationDirection < 0 || parameters.integrationDirection > 2)
        return {FeatureError::InvalidParameter, "Integration direction must be 0, 1 or 2"};
    // 0 RK2, 1 RK4, 2 RK45 (adaptive Cash-Karp), 3 RK5 (fixed-step).
    if (parameters.integrator < 0 || parameters.integrator > 3)
        return {FeatureError::InvalidParameter,
                "Integrator must be 0 (RK2), 1 (RK4), 2 (RK45) or 3 (RK5)"};
    if (parameters.stepUnit != 1 && parameters.stepUnit != 2)
        return {FeatureError::InvalidParameter, "Step unit must be a length or a cell length"};
    if (!std::isfinite(parameters.maximumPropagation) || parameters.maximumPropagation <= 0.0)
        return {FeatureError::InvalidParameter, "Maximum propagation must be positive"};
    if (!std::isfinite(parameters.initialStep) || parameters.initialStep < 0.0)
        return {FeatureError::InvalidParameter, "Initial step cannot be negative"};
    if (!std::isfinite(parameters.minimumStep) || parameters.minimumStep < 0.0)
        return {FeatureError::InvalidParameter, "Minimum step cannot be negative"};
    if (!std::isfinite(parameters.maximumStep) || parameters.maximumStep < 0.0)
        return {FeatureError::InvalidParameter, "Maximum step cannot be negative"};
    if (parameters.minimumStep > 0.0 && parameters.maximumStep > 0.0
        && parameters.minimumStep > parameters.maximumStep)
        return {FeatureError::InvalidParameter, "Minimum step exceeds maximum step"};
    if (parameters.maximumSteps < 1)
        return {FeatureError::InvalidParameter, "Maximum steps must be at least 1"};
    if (!std::isfinite(parameters.terminalSpeed) || parameters.terminalSpeed < 0.0)
        return {FeatureError::InvalidParameter, "Terminal speed cannot be negative"};

    const StreamlineSeeds &seeds = parameters.seeds;
    if (seeds.mode == SeedMode::ExplicitPoints) {
        if (seeds.points.empty())
            return {FeatureError::NoSeeds, "Explicit seeds require at least one point"};
        for (const auto &point : seeds.points) {
            for (double coordinate : point) {
                if (!std::isfinite(coordinate))
                    return {FeatureError::NonFiniteData, "Non-finite seed coordinate"};
            }
        }
    } else if (seeds.mode == SeedMode::Line) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(seeds.lineStart[axis]) || !std::isfinite(seeds.lineEnd[axis]))
                return {FeatureError::NonFiniteData, "Non-finite seed coordinate"};
        }
        if (seeds.lineResolution < 2)
            return {FeatureError::InvalidParameter, "Line seeds require resolution of 2 or more"};
    } else if (seeds.surfaceStride < 1) {
        return {FeatureError::InvalidParameter, "Surface seed stride must be 1 or more"};
    }
    return {};
}

FeatureStatus validateFilter(const StreamlineFilter &filter)
{
    if (!std::isfinite(filter.minimumArcLength) || filter.minimumArcLength < 0.0)
        return {FeatureError::InvalidParameter, "Minimum arc length cannot be negative"};
    if (!std::isfinite(filter.dedupDistanceTolerance) || filter.dedupDistanceTolerance < 0.0)
        return {FeatureError::InvalidParameter, "Dedup distance tolerance cannot be negative"};
    if (!std::isfinite(filter.dedupAngleToleranceDeg) || filter.dedupAngleToleranceDeg < 0.0
        || filter.dedupAngleToleranceDeg > 180.0)
        return {FeatureError::InvalidParameter, "Dedup angle tolerance must be within 0..180"};
    return {};
}

// Resolves the caller's named array instead of "the first vector array": a mesh
// may carry several and a silent pick would trace the wrong field.
FeatureStatus resolveVelocity(vtkDataSet *input, const std::string &name,
                              int &association)
{
    vtkDataArray *onPoints = input->GetPointData()->GetArray(name.c_str());
    vtkDataArray *onCells = input->GetCellData()->GetArray(name.c_str());
    if (!onPoints && !onCells)
        return {FeatureError::MissingField, "Velocity field not found on points or cells"};
    vtkDataArray *array = onPoints ? onPoints : onCells;
    association = onPoints ? vtkDataObject::FIELD_ASSOCIATION_POINTS
                           : vtkDataObject::FIELD_ASSOCIATION_CELLS;
    if (array->GetNumberOfComponents() != 3)
        return {FeatureError::MissingField, "Velocity field must have three components"};
    const vtkIdType expected = onPoints ? input->GetNumberOfPoints()
                                        : input->GetNumberOfCells();
    if (array->GetNumberOfTuples() != expected)
        return {FeatureError::MissingField, "Velocity field does not match the dataset size"};
    for (vtkIdType i = 0; i < expected; ++i) {
        double vector[3];
        array->GetTuple(i, vector);
        for (double component : vector) {
            if (!std::isfinite(component))
                return {FeatureError::NonFiniteData, "Velocity field contains NaN or Inf"};
        }
    }
    return {};
}

struct SeedCandidates {
    std::vector<std::array<double, 3>> points;
    // Index back into the caller-visible seed order, so a rejection can name the
    // seed the caller actually supplied even after compaction.
    std::vector<int> source;
};

SeedCandidates expandSeeds(const StreamlineSeeds &seeds, vtkDataSet *surface)
{
    SeedCandidates candidates;
    if (seeds.mode == SeedMode::ExplicitPoints) {
        candidates.points = seeds.points;
        candidates.source.reserve(seeds.points.size());
        for (std::size_t i = 0; i < seeds.points.size(); ++i)
            candidates.source.push_back(static_cast<int>(i));
        return candidates;
    }
    if (seeds.mode == SeedMode::Line) {
        const int resolution = seeds.lineResolution;
        candidates.points.reserve(static_cast<std::size_t>(resolution));
        for (int i = 0; i < resolution; ++i) {
            const double t = static_cast<double>(i) / (resolution - 1);
            std::array<double, 3> point{};
            for (int axis = 0; axis < 3; ++axis)
                point[axis] = seeds.lineStart[axis] * (1.0 - t) + seeds.lineEnd[axis] * t;
            candidates.points.push_back(point);
            candidates.source.push_back(i);
        }
        return candidates;
    }
    const vtkIdType count = surface->GetNumberOfPoints();
    for (vtkIdType i = 0; i < count; i += seeds.surfaceStride) {
        double point[3];
        surface->GetPoint(i, point);
        if (std::isfinite(point[0]) && std::isfinite(point[1]) && std::isfinite(point[2])) {
            candidates.points.push_back(std::array<double, 3>{{point[0], point[1], point[2]}});
            candidates.source.push_back(static_cast<int>(i));
        }
    }
    return candidates;
}

// Unit normal of a polygonal cell, from its first three points. vtkCell has no
// ComputeNormal; only vtkPolygon and vtkTriangle carry static helpers.
bool cellNormal(vtkCell *cell, double normal[3])
{
    if (!cell || cell->GetNumberOfPoints() < 3)
        return false;
    double p0[3];
    double p1[3];
    double p2[3];
    cell->GetPoints()->GetPoint(0, p0);
    cell->GetPoints()->GetPoint(1, p1);
    cell->GetPoints()->GetPoint(2, p2);
    double u[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
    double v[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
    vtkMath::Cross(u, v, normal);
    const double norm = vtkMath::Norm(normal);
    if (!(norm > 0.0) || !std::isfinite(norm))
        return false;
    normal[0] /= norm;
    normal[1] /= norm;
    normal[2] /= norm;
    return true;
}

// Velocity at an arbitrary point inside a cell. Control returns false when the
// cell cannot place the point, in which case the caller must not judge it.
bool interpolatedVelocity(vtkDataSet *input, vtkDataArray *velocity, vtkCell *cell,
                          const double x[3], double out[3])
{
    if (!velocity || !cell)
        return false;
    const int count = cell->GetNumberOfPoints();
    if (count < 1 || velocity->GetNumberOfComponents() != 3)
        return false;
    if (velocity->GetNumberOfTuples() < input->GetNumberOfPoints())
        return false;
    std::vector<double> weights(static_cast<std::size_t>(count), 0.0);
    double closest[3];
    double pcoords[3];
    double distance2 = 0.0;
    int subId = 0;
    if (cell->EvaluatePosition(x, closest, subId, pcoords, distance2, weights.data()) != 1)
        return false;
    double value[3] = {0.0, 0.0, 0.0};
    for (int i = 0; i < count; ++i) {
        double tuple[3];
        velocity->GetTuple(cell->GetPointId(i), tuple);
        value[0] += weights[static_cast<std::size_t>(i)] * tuple[0];
        value[1] += weights[static_cast<std::size_t>(i)] * tuple[1];
        value[2] += weights[static_cast<std::size_t>(i)] * tuple[2];
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(value[axis]))
            return false;
    }
    out[0] = value[0];
    out[1] = value[1];
    out[2] = value[2];
    return true;
}

// Drops seeds that cannot start a line and records why. VTK silently discards
// lines of a single point (vtkStreamTracer.cxx:1136), so a seed outside the
// domain would otherwise leave no trace in the output at all.
FeatureStatus filterSeeds(vtkDataSet *surface, vtkDataArray *velocity,
                          const StreamlineFilter &filter, SeedCandidates &candidates,
                          std::vector<RejectedLine> &rejected)
{
    if (candidates.points.empty())
        return {FeatureError::NoSeeds, "No seed point survived preparation"};

    vtkNew<vtkCellLocator> locator;
    locator->SetDataSet(surface);
    locator->BuildLocator();

    double bounds[6];
    surface->GetBounds(bounds);
    double extent2 = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
        const double span = bounds[2 * axis + 1] - bounds[2 * axis];
        extent2 += span * span;
    }
    const double tolerance = std::sqrt(extent2) * 1.0e-6;

    vtkNew<vtkGenericCell> generic;
    std::vector<std::array<double, 3>> keptPoints;
    std::vector<int> keptSource;
    keptPoints.reserve(candidates.points.size());
    keptSource.reserve(candidates.points.size());

    for (std::size_t i = 0; i < candidates.points.size(); ++i) {
        const std::array<double, 3> &point = candidates.points[i];
        const double x[3] = {point[0], point[1], point[2]};
        double closest[3];
        vtkIdType cellId = -1;
        int subId = 0;
        double distance2 = 0.0;
        locator->FindClosestPoint(x, closest, generic, cellId, subId, distance2);
        if (cellId < 0 || !std::isfinite(distance2) || distance2 > tolerance * tolerance) {
            RejectedLine entry;
            entry.reason = RejectedLine::Reason::seedOutsideDomain;
            entry.seedIndex = candidates.source[i];
            entry.detail = "Seed does not lie on the selected surface";
            rejected.push_back(entry);
            continue;
        }
        if (filter.dropParallelToNormal && velocity) {
            double normal[3];
            if (cellNormal(generic, normal)) {
                double value[3];
                if (interpolatedVelocity(surface, velocity, generic, x, value)) {
                    const double along = vtkMath::Dot(normal, value);
                    double tangent[3] = {value[0] - normal[0] * along,
                                         value[1] - normal[1] * along,
                                         value[2] - normal[2] * along};
                    if (vtkMath::Norm(tangent) < 1.0e-9) {
                        RejectedLine entry;
                        entry.reason = RejectedLine::Reason::parallelToSurfaceNormal;
                        entry.seedIndex = candidates.source[i];
                        entry.detail = "Velocity at the seed is parallel to the surface normal";
                        rejected.push_back(entry);
                        continue;
                    }
                }
            }
        }
        keptPoints.push_back(point);
        keptSource.push_back(candidates.source[i]);
    }

    if (keptPoints.empty()) {
        if (!rejected.empty()
            && rejected.front().reason == RejectedLine::Reason::seedOutsideDomain)
            return {FeatureError::SeedOutsideDomain, "No seed lies on the selected surface"};
        return {FeatureError::NoSeeds, "No seed can start a streamline"};
    }
    candidates.points.swap(keptPoints);
    candidates.source.swap(keptSource);
    return {};
}

vtkSmartPointer<vtkPolyData> buildSeedData(const std::vector<std::array<double, 3>> &points)
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

// The tracer's setters take double, but the caller may legitimately pass 0 to
// mean "let VTK choose", so no clamping happens here.
void configureTracer(vtkStreamTracer *tracer, const StreamlineParameters &parameters,
                     int association, vtkPolyData *seeds)
{
    tracer->SetInputArrayToProcess(0, 0, 0, association, parameters.velocityField.c_str());
    tracer->SetSourceData(seeds);
    // SetIntegratorType only recognises 0..2 and silently keeps the previous
    // solver for anything else, so the solver is instantiated here.
    vtkSmartPointer<vtkInitialValueProblemSolver> integrator =
        createIntegrator(parameters.integrator);
    tracer->SetIntegrator(integrator);
    tracer->SetIntegrationDirection(parameters.integrationDirection);
    tracer->SetMaximumPropagation(parameters.maximumPropagation);
    tracer->SetInitialIntegrationStep(parameters.initialStep);
    tracer->SetMinimumIntegrationStep(parameters.minimumStep);
    tracer->SetMaximumIntegrationStep(parameters.maximumStep);
    tracer->SetIntegrationStepUnit(parameters.stepUnit);
    tracer->SetMaximumNumberOfSteps(static_cast<vtkIdType>(parameters.maximumSteps));
    tracer->SetTerminalSpeed(parameters.terminalSpeed);
    // Surface projection is only honoured by the point-locator velocity field;
    // with the cell locator VTK prints a warning and switches projection off
    // (vtkStreamTracer.cxx:702-718).
    tracer->SetInterpolatorTypeToDataSetPointLocator();
}

struct ParsedLine {
    std::vector<vtkIdType> ids;
    StreamlineTermination termination = StreamlineTermination::Unknown;
    int seedIndex = -1;
    double arcLength = 0.0;
    std::array<double, 3> start{{0.0, 0.0, 0.0}};
    std::array<double, 3> direction{{0.0, 0.0, 0.0}};
};

double polylineLength(vtkPoints *points, vtkIdType count, const vtkIdType *ids)
{
    double length = 0.0;
    for (vtkIdType i = 1; i < count; ++i) {
        double previous[3];
        double current[3];
        points->GetPoint(ids[i - 1], previous);
        points->GetPoint(ids[i], current);
        length += std::sqrt(vtkMath::Distance2BetweenPoints(previous, current));
    }
    return length;
}

double angleDegrees(const std::array<double, 3> &a, const std::array<double, 3> &b)
{
    double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    dot = std::max(-1.0, std::min(1.0, dot));
    return std::acos(dot) * 180.0 / vtkMath::Pi();
}

// One cell per polyline means cell data runs parallel to the lines, which is how
// the termination reason and seed id are recovered (vtkStreamTracer.cxx:1179).
std::vector<ParsedLine> parseLines(vtkPolyData *output, vtkIdType seedCount,
                                   const std::vector<int> &seedSource)
{
    std::vector<ParsedLine> parsed;
    vtkPoints *points = output->GetPoints();
    vtkCellArray *lines = output->GetLines();
    if (!points || !lines)
        return parsed;
    vtkIntArray *reasons = vtkIntArray::SafeDownCast(
        output->GetCellData()->GetArray("ReasonForTermination"));
    vtkIntArray *seedIds = vtkIntArray::SafeDownCast(
        output->GetCellData()->GetArray("SeedIds"));
    vtkIdType cellId = 0;
    lines->InitTraversal();
    for (vtkIdType npts = 0, *ids = nullptr; lines->GetNextCell(npts, ids); ++cellId) {
        ParsedLine line;
        line.ids.assign(ids, ids + npts);
        line.arcLength = polylineLength(points, npts, ids);
        if (reasons && cellId < reasons->GetNumberOfTuples()) {
            const int value = reasons->GetValue(cellId);
            if (value >= static_cast<int>(StreamlineTermination::OutOfDomain)
                && value <= static_cast<int>(StreamlineTermination::Stagnation))
                line.termination = static_cast<StreamlineTermination>(value);
        }
        if (seedIds && cellId < seedIds->GetNumberOfTuples() && seedCount > 0) {
            int value = seedIds->GetValue(cellId);
            // "Both" integrates every seed forward, then backward, so the second
            // half of the ids repeats the first (vtkStreamTracer.cxx:330-338).
            if (value >= seedCount)
                value -= static_cast<int>(seedCount);
            if (value >= 0 && static_cast<std::size_t>(value) < seedSource.size())
                line.seedIndex = seedSource[static_cast<std::size_t>(value)];
        }
        if (npts > 0) {
            double first[3];
            double last[3];
            points->GetPoint(ids[0], first);
            points->GetPoint(ids[npts - 1], last);
            line.start = {{first[0], first[1], first[2]}};
            const double delta[3] = {last[0] - first[0], last[1] - first[1], last[2] - first[2]};
            const double norm = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1]
                                          + delta[2] * delta[2]);
            if (norm > 0.0)
                line.direction = {{delta[0] / norm, delta[1] / norm, delta[2] / norm}};
        }
        parsed.push_back(line);
    }
    return parsed;
}

StreamlineResult filterLines(vtkPolyData *output, vtkIdType seedCount,
                             const std::vector<int> &seedSource,
                             const StreamlineFilter &filter,
                             std::vector<StreamlineTermination> &terminations,
                             std::vector<RejectedLine> &rejected,
                             std::size_t &producedCount)
{
    const std::vector<ParsedLine> parsed = parseLines(output, seedCount, seedSource);
    producedCount = parsed.size();
    if (parsed.empty()) {
        StreamlineResult result = failure(FeatureError::EmptyResult,
                                          "The tracer produced no streamline");
        result.producedCount = producedCount;
        result.rejected = rejected;
        return result;
    }

    // Rule 1 and rule 2 are applied in a single pass, before any reordering.
    std::vector<std::size_t> accepted;
    accepted.reserve(parsed.size());
    for (std::size_t i = 0; i < parsed.size(); ++i) {
        const ParsedLine &line = parsed[i];
        if (line.ids.size() < 2 || !(line.arcLength > 0.0)) {
            RejectedLine entry;
            entry.reason = RejectedLine::Reason::zeroLength;
            entry.seedIndex = line.seedIndex;
            entry.detail = "Line has fewer than two points or zero length";
            rejected.push_back(entry);
            continue;
        }
        if (filter.minimumArcLength > 0.0 && line.arcLength < filter.minimumArcLength) {
            RejectedLine entry;
            entry.reason = RejectedLine::Reason::belowMinLength;
            entry.seedIndex = line.seedIndex;
            entry.arcLength = line.arcLength;
            entry.detail = "Line is shorter than the minimum arc length";
            rejected.push_back(entry);
            continue;
        }
        accepted.push_back(i);
    }

    // Rule 5: overlapping seeds would otherwise draw the same line twice.
    if (filter.deduplicateOverlapping) {
        std::vector<std::size_t> unique;
        unique.reserve(accepted.size());
        for (std::size_t index : accepted) {
            const ParsedLine &line = parsed[index];
            bool duplicate = false;
            for (std::size_t kept : unique) {
                const ParsedLine &other = parsed[kept];
                const double delta[3] = {line.start[0] - other.start[0],
                                         line.start[1] - other.start[1],
                                         line.start[2] - other.start[2]};
                const double distance = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1]
                                                  + delta[2] * delta[2]);
                if (distance > filter.dedupDistanceTolerance)
                    continue;
                if (angleDegrees(line.direction, other.direction)
                    > filter.dedupAngleToleranceDeg)
                    continue;
                duplicate = true;
                break;
            }
            if (!duplicate) {
                unique.push_back(index);
                continue;
            }
            RejectedLine entry;
            entry.reason = RejectedLine::Reason::duplicate;
            entry.seedIndex = line.seedIndex;
            entry.arcLength = line.arcLength;
            entry.detail = "Line shares a start point and direction with a kept line";
            rejected.push_back(entry);
        }
        accepted.swap(unique);
    }

    // Rule 6: keep the longest lines when the budget is exceeded, then restore
    // the tracer's order so the output stays deterministic.
    if (filter.maximumLines > 0 && accepted.size() > filter.maximumLines) {
        std::vector<std::size_t> ranked = accepted;
        std::stable_sort(ranked.begin(), ranked.end(),
                         [&parsed](std::size_t left, std::size_t right) {
                             return parsed[left].arcLength > parsed[right].arcLength;
                         });
        ranked.resize(filter.maximumLines);
        std::vector<bool> keep(parsed.size(), false);
        for (std::size_t index : ranked)
            keep[index] = true;
        std::vector<std::size_t> survivors;
        survivors.reserve(ranked.size());
        for (std::size_t index : accepted) {
            if (keep[index]) {
                survivors.push_back(index);
                continue;
            }
            RejectedLine entry;
            entry.reason = RejectedLine::Reason::overLineLimit;
            entry.seedIndex = parsed[index].seedIndex;
            entry.arcLength = parsed[index].arcLength;
            entry.detail = "Line exceeds the maximum line count";
            rejected.push_back(entry);
        }
        accepted.swap(survivors);
    }

    if (accepted.empty()) {
        StreamlineResult result = failure(FeatureError::EmptyResult,
                                          "Every traced line was removed by the filters");
        result.producedCount = producedCount;
        result.rejected = rejected;
        return result;
    }

    // Rebuild with compacted points so the snapshot holds no unused vertices and
    // pointCount() reports what the caller actually sees.
    vtkNew<vtkPoints> keptPoints;
    keptPoints->SetDataTypeToDouble();
    vtkNew<vtkCellArray> keptLines;
    vtkPoints *source = output->GetPoints();
    for (std::size_t index : accepted) {
        const ParsedLine &line = parsed[index];
        std::vector<vtkIdType> ids;
        ids.reserve(line.ids.size());
        for (vtkIdType id : line.ids) {
            double point[3];
            source->GetPoint(id, point);
            ids.push_back(keptPoints->InsertNextPoint(point));
        }
        keptLines->InsertNextCell(static_cast<vtkIdType>(ids.size()), ids.data());
        terminations.push_back(line.termination);
    }
    vtkNew<vtkPolyData> kept;
    kept->SetPoints(keptPoints);
    kept->SetLines(keptLines);

    const FeatureResult snapshot = FeatureProcessor::snapshot(kept);
    if (!snapshot.ok()) {
        StreamlineResult result;
        result.status = snapshot.status;
        result.producedCount = producedCount;
        result.rejected = rejected;
        return result;
    }
    StreamlineResult result;
    result.status = snapshot.status;
    result.geometry = snapshot.geometry;
    result.producedCount = producedCount;
    result.rejected = rejected;
    return result;
}

StreamlineResult runTrace(vtkDataSet *input, const StreamlineParameters &parameters,
                          const StreamlineFilter &filter, bool projectOnSurface)
{
    auto status = validateInput(input, projectOnSurface ? 2 : 3);
    if (!status.ok())
        return failure(status.code, status.message);
    status = validateParameters(parameters);
    if (!status.ok())
        return failure(status.code, status.message);
    status = validateFilter(filter);
    if (!status.ok())
        return failure(status.code, status.message);
    int association = 0;
    status = resolveVelocity(input, parameters.velocityField, association);
    if (!status.ok())
        return failure(status.code, status.message);
    vtkDataArray *velocity = association == vtkDataObject::FIELD_ASSOCIATION_POINTS
        ? input->GetPointData()->GetArray(parameters.velocityField.c_str())
        : input->GetCellData()->GetArray(parameters.velocityField.c_str());

    SeedCandidates candidates = expandSeeds(parameters.seeds, input);
    std::vector<RejectedLine> rejected;
    if (projectOnSurface) {
        status = filterSeeds(input, velocity, filter, candidates, rejected);
        if (!status.ok()) {
            // Keep the per-seed reasons: a caller that supplied several seeds
            // needs to see which ones were dropped and why.
            StreamlineResult result = failure(status.code, status.message);
            result.rejected = rejected;
            return result;
        }
    } else if (candidates.points.empty()) {
        return failure(FeatureError::NoSeeds, "No seed point survived preparation");
    }

    vtkSmartPointer<vtkPolyData> seeds = buildSeedData(candidates.points);

    vtkNew<vtkStreamTracer> tracer;
    tracer->SetInputData(input);
    configureTracer(tracer, parameters, association, seeds);
    // Only the wall variant projects; a free-space trace must keep its 3D
    // direction or it would drift along the first cell's plane.
    tracer->SetSurfaceStreamlines(projectOnSurface && parameters.surfaceStreamlines);
    tracer->Update();

    std::vector<StreamlineTermination> terminations;
    std::size_t producedCount = 0;
    StreamlineResult result = filterLines(tracer->GetOutput(),
                                          static_cast<vtkIdType>(candidates.points.size()),
                                          candidates.source, filter, terminations,
                                          rejected, producedCount);
    result.terminations = terminations;
    result.rejected = rejected;
    result.producedCount = producedCount;
    return result;
}

} // namespace

StreamlineResult surfaceStreamlines(vtkDataSet *surface, const StreamlineParameters &parameters,
                                    const StreamlineFilter &filter)
{
    return runTrace(surface, parameters, filter, true);
}

StreamlineResult volumeStreamlines(vtkDataSet *volume, const StreamlineParameters &parameters,
                                   const StreamlineFilter &filter)
{
    return runTrace(volume, parameters, filter, false);
}

const char *streamlineTerminationName(StreamlineTermination termination)
{
    switch (termination) {
    case StreamlineTermination::OutOfDomain: return "outOfDomain";
    case StreamlineTermination::NotInitialized: return "notInitialized";
    case StreamlineTermination::UnexpectedValue: return "unexpectedValue";
    case StreamlineTermination::OutOfLength: return "outOfLength";
    case StreamlineTermination::OutOfSteps: return "outOfSteps";
    case StreamlineTermination::Stagnation: return "stagnation";
    case StreamlineTermination::Unknown: break;
    }
    return "unknown";
}

const char *rejectedLineReasonName(RejectedLine::Reason reason)
{
    switch (reason) {
    case RejectedLine::Reason::zeroLength: return "zeroLength";
    case RejectedLine::Reason::belowMinLength: return "belowMinLength";
    case RejectedLine::Reason::seedOutsideDomain: return "seedOutsideDomain";
    case RejectedLine::Reason::parallelToSurfaceNormal: return "parallelToSurfaceNormal";
    case RejectedLine::Reason::duplicate: return "duplicate";
    case RejectedLine::Reason::overLineLimit: return "overLineLimit";
    }
    return "unknown";
}

} // namespace visualization
} // namespace aerosim