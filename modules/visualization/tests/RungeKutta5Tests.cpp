#include "aerosim/visualization/features/RungeKutta5.h"

#include "aerosim/visualization/features/StreamlineProcessor.h"

#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkFunctionSet.h>
#include <vtkObjectBase.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkVersion.h>

#include <QCoreApplication>
#include <QtGlobal>

#include <array>
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

// The test system y(t) = exp(t) obeys y' = y with y(0) = 1, so a solver can be
// scored against the exact value instead of merely observed to move. Limit makes
// FunctionValues fail past a time, which is how VTK signals leaving the domain.
class ExponentialFunctionSet : public vtkFunctionSet
{
public:
    static ExponentialFunctionSet *New() { return new ExponentialFunctionSet; }

    int FunctionValues(double *x, double *f) override
    {
        if (x[1] > this->Limit)
            return 0;
        f[0] = x[0];
        return 1;
    }

    double Limit = 1.0e300;

protected:
    ExponentialFunctionSet()
    {
        this->NumFuncs = 1;
        this->NumIndepVars = 2;
    }
};

vtkSmartPointer<ExponentialFunctionSet> makeFunctionSet(double limit = 1.0e300)
{
    vtkSmartPointer<ExponentialFunctionSet> functions =
        vtkSmartPointer<ExponentialFunctionSet>::New();
    functions->Limit = limit;
    return functions;
}

// Integrates y' = y from t = 0 to t = 1 with a fixed step and returns y(1).
double integrateToOne(double step, int code)
{
    vtkSmartPointer<vtkInitialValueProblemSolver> solver = createIntegrator(code);
    require(solver.GetPointer() != nullptr, "Integrator could not be created");
    vtkSmartPointer<ExponentialFunctionSet> functions = makeFunctionSet();
    solver->SetFunctionSet(functions);

    const int steps = static_cast<int>(std::lround(1.0 / step));
    double value = 1.0;
    double time = 0.0;
    for (int i = 0; i < steps; ++i) {
        double next = 0.0;
        double delT = step;
        double delTActual = 0.0;
        double localError = 0.0;
        const int status = solver->ComputeNextStep(&value, &next, time, delT, delTActual,
                                                  step, step, 1.0e-6, localError);
        require(status == 0, "A fixed-step integration step failed");
        require(delTActual == step, "A non-adaptive solver must take the whole step");
        value = next;
        time += delTActual;
    }
    return value;
}

// Flat plate at z = 0 carrying a uniform in-plane velocity. It proves each
// integrator code is accepted by the surface streamline kernel end to end.
vtkSmartPointer<vtkPolyData> plate()
{
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    vtkSmartPointer<vtkDoubleArray> velocity = vtkSmartPointer<vtkDoubleArray>::New();
    velocity->SetName("Velocity");
    velocity->SetNumberOfComponents(3);
    for (int j = 0; j < 3; ++j) {
        for (int i = 0; i < 3; ++i) {
            points->InsertNextPoint(-1.0 + (double)i, -1.0 + (double)j, 0.0);
            velocity->InsertNextTuple3(1.0, 0.3, 0.0);
        }
    }
    vtkSmartPointer<vtkCellArray> quads = vtkSmartPointer<vtkCellArray>::New();
    for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 2; ++i) {
            vtkIdType quad[4] = {j * 3 + i, j * 3 + i + 1, (j + 1) * 3 + i + 1,
                                 (j + 1) * 3 + i};
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
    std::cout << "C++14 / Qt " << qVersion() << " / VTK " << vtkVersion::GetVTKVersion()
              << std::endl;
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

    const double exact = std::exp(1.0);

    test("RK5 integrates a linear system to fifth-order accuracy", [&] {
        const double fifth = std::abs(integrateToOne(0.1, IntegratorRungeKutta5) - exact);
        const double fourth = std::abs(integrateToOne(0.1, IntegratorRungeKutta4) - exact);
        require(fifth < 1.0e-7, "RK5 error exceeds what a fifth-order method allows");
        // The measured ratio at this step is about 550; 100 still catches an RK4
        // published under the RK5 name while tolerating a differently shaped but
        // equally valid tableau.
        require(fourth / fifth > 100.0, "RK5 is not markedly more accurate than RK4");
    });

    test("RK5 error falls at the fifth order when the step halves", [&] {
        const double coarse = std::abs(integrateToOne(0.1, IntegratorRungeKutta5) - exact);
        const double fine = std::abs(integrateToOne(0.05, IntegratorRungeKutta5) - exact);
        const double ratio = coarse / fine;
        // A fifth-order method gains 2^5 = 32 from halving the step.
        require(ratio > 20.0 && ratio < 45.0,
                "The convergence ratio does not match a fifth-order method");
    });

    test("RK5 advertises itself as fixed step while RK45 stays adaptive", [&] {
        auto rk5 = createIntegrator(IntegratorRungeKutta5);
        require(rk5->IsAdaptive() == 0, "RK5 must not advertise adaptivity");
        auto rk45 = createIntegrator(IntegratorRungeKutta45);
        require(rk45->IsAdaptive() != 0, "RK45 is the adaptive solver");
        auto rk4 = createIntegrator(IntegratorRungeKutta4);
        require(rk4->IsAdaptive() == 0, "RK4 is also a fixed-step solver");
    });

    test("RK5 accepts a supplied starting derivative without changing the result",
         [&] {
             vtkSmartPointer<ExponentialFunctionSet> functions = makeFunctionSet();
             auto withDerivative = createIntegrator(IntegratorRungeKutta5);
             withDerivative->SetFunctionSet(functions);
             auto withoutDerivative = createIntegrator(IntegratorRungeKutta5);
             withoutDerivative->SetFunctionSet(functions);

             double start = 2.0;
             double derivative = 2.0;
             double plain = 0.0;
             double seeded = 0.0;
             double delT = 0.1;
             double delTActual = 0.0;
             double error = 0.0;
             require(withoutDerivative->ComputeNextStep(&start, &plain, 0.0, delT, delTActual,
                                                        0.1, 0.1, 1e-6, error) == 0,
                     "Step without a starting derivative failed");
             require(withDerivative->ComputeNextStep(&start, &derivative, &seeded, 0.0, delT,
                                                     delTActual, 0.1, 0.1, 1e-6, error) == 0,
                     "Step with a starting derivative failed");
             require(std::abs(plain - seeded) < 1e-15,
                     "A supplied starting derivative changed the result");
         });

    test("RK5 reports a domain exit together with the progress it made", [&] {
        auto solver = createIntegrator(IntegratorRungeKutta5);
        vtkSmartPointer<ExponentialFunctionSet> functions = makeFunctionSet(0.5);
        solver->SetFunctionSet(functions);
        // Starting at the limit the first stage still evaluates; the second stage
        // probes t + 0.25 * step, which is already outside.
        double value = std::exp(0.5);
        double next = 0.0;
        double delT = 0.1;
        double delTActual = -1.0;
        double error = 0.0;
        const int status =
            solver->ComputeNextStep(&value, &next, 0.5, delT, delTActual, 0.1, 0.1, 1e-6, error);
        require(status == 1, "Leaving the domain must report OUT_OF_DOMAIN");
        require(std::abs(delTActual - 0.025) < 1e-12,
                "The reported progress must match the completed stages");
    });

    test("RK5 refuses a zero step and an uninitialised function set", [&] {
        auto uninitialised = createIntegrator(IntegratorRungeKutta5);
        double value = 1.0;
        double next = 0.0;
        double delT = 0.1;
        double delTActual = 0.0;
        double error = 0.0;
        require(uninitialised->ComputeNextStep(&value, &next, 0.0, delT, delTActual, 0.1,
                                               0.1, 1e-6, error) == 2,
                "A missing function set must report NOT_INITIALIZED");

        auto solver = createIntegrator(IntegratorRungeKutta5);
        vtkSmartPointer<ExponentialFunctionSet> functions = makeFunctionSet();
        solver->SetFunctionSet(functions);
        delT = 0.0;
        require(solver->ComputeNextStep(&value, &next, 0.0, delT, delTActual, 0.0, 0.0,
                                        1e-6, error) == 3,
                "A zero step must report UNEXPECTED_VALUE");
        require(next == value, "A refused step must not move the point");
    });

    test("createIntegrator maps every code and rejects unknown ones", [&] {
        // Code 2 deliberately resolves to the bounded subclass: vtkParticleTracerBase
        // always forwards minStep = maxStep = 0, which makes the stock adaptive solver
        // collapse the step to zero (errRatio == 0 -> tmp = minStep). The subclass
        // repairs those bounds from the requested step, so the class name is part of
        // the contract rather than an implementation detail.
        const char *expected[4] = {"vtkRungeKutta2", "vtkRungeKutta4", "BoundedRungeKutta45",
                                   "RungeKutta5"};
        for (int code = 0; code < 4; ++code) {
            auto solver = createIntegrator(code);
            require(solver.GetPointer() != nullptr, "A known code must build a solver");
            require(std::string(solver->GetClassName()) == expected[code],
                    "Wrong solver class for code " + std::to_string(code));
        }

        // The bounded solver must stay an adaptive vtkRungeKutta45, otherwise the
        // step-control behaviour that the repair depends on would be gone.
        auto bounded = createIntegrator(IntegratorRungeKutta45);
        require(bounded->IsA("vtkRungeKutta45"),
                "The bounded solver must derive from vtkRungeKutta45");
        require(bounded->IsAdaptive() != 0, "The bounded solver must stay adaptive");
        require(createIntegrator(4).GetPointer() == nullptr, "Code 4 must not build a solver");
        require(createIntegrator(-1).GetPointer() == nullptr,
                "A negative code must be rejected");

        require(std::string(integratorName(0)) == "RK2", "Wrong name for code 0");
        require(std::string(integratorName(2)) == "RK45", "Wrong name for code 2");
        require(std::string(integratorName(3)) == "RK5", "Wrong name for code 3");
        require(std::string(integratorName(9)) == "unknown",
                "Wrong name for an unknown code");
    });

    test("RK5 clones through NewInstance like the native solvers", [&] {
        // Both tracers clone the integrator with NewInstance() inside RequestData,
        // so a solver that cannot be cloned would silently lose its type.
        auto prototype = createIntegrator(IntegratorRungeKutta5);
        vtkSmartPointer<vtkObjectBase> clone;
        clone.TakeReference(prototype->NewInstance());
        require(clone.GetPointer() != nullptr, "NewInstance returned nothing");
        require(clone->IsA("RungeKutta5"), "The clone lost its class");
    });

    test("surface streamlines accept every integrator code including RK5", [&] {
        for (int code = 0; code < 4; ++code) {
            auto surface = plate();
            StreamlineParameters parameters;
            parameters.integrator = code;
            parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, -1.0, 0.0}});
            auto result = surfaceStreamlines(surface, parameters);
            require(result.ok(), std::string("Integrator ") + integratorName(code)
                                     + " was rejected: " + result.status.message);
            require(result.acceptedCount() >= 1, "No line was produced");
            auto lines = result.geometry->copyData();
            require(lines->GetNumberOfLines() >= 1, "The result has no polyline");
        }
    });

    test("an out-of-range integrator is rejected with a helpful message", [&] {
        auto surface = plate();
        StreamlineParameters parameters;
        parameters.integrator = 4;
        parameters.seeds.points.push_back(std::array<double, 3>{{-1.0, -1.0, 0.0}});
        auto result = surfaceStreamlines(surface, parameters);
        require(!result.ok(), "Code 4 must be rejected");
        require(result.status.code == FeatureError::InvalidParameter,
                "Wrong error code for an out-of-range integrator");
        require(result.status.message.find("RK5") != std::string::npos,
                "The message must list the supported integrators");
    });

    std::cout << "RESULT: " << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}