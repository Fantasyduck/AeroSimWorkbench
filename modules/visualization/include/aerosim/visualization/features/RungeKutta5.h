#ifndef AEROSIM_VISUALIZATION_FEATURES_RUNGEKUTTA5_H
#define AEROSIM_VISUALIZATION_FEATURES_RUNGEKUTTA5_H

#include <vtkInitialValueProblemSolver.h>
#include <vtkRungeKutta45.h>
#include <vtkSmartPointer.h>

namespace aerosim {
namespace visualization {

// Fixed-step explicit fifth-order Runge-Kutta solver (the classic six-stage
// Butcher tableau). VTK 8.2 ships vtkRungeKutta2, vtkRungeKutta4 and
// vtkRungeKutta45 but no fifth-order fixed-step solver: vtkRungeKutta45 is an
// adaptive Cash-Karp 4(5) pair that carries a fifth-order formula but chooses
// its step from the embedded fourth-order error estimate and re-clamps that
// step on every call. Callers that ask for "RK5" mean this fixed-step method,
// so it is implemented here rather than aliased onto RK45, which would silently
// return a different algorithm.
//
// The class mirrors vtkRungeKutta4 exactly: vtkTypeMacro supplies
// NewInstanceInternal(), so vtkStreamTracer and vtkParticleTracerBase clone it
// through NewInstance() the same way they clone the native solvers.
class RungeKutta5 : public vtkInitialValueProblemSolver
{
public:
    vtkTypeMacro(RungeKutta5, vtkInitialValueProblemSolver);
    void PrintSelf(ostream &os, vtkIndent indent) override;

    // Construct a RungeKutta5 with no initial FunctionSet.
    static RungeKutta5 *New();

    // The three convenience overloads forward to the full one, exactly as the
    // VTK base class does, so a caller may use whichever signature it prefers.
    int ComputeNextStep(double *xprev, double *xnext, double t, double &delT,
                        double maxError, double &error) override
    {
        double minStep = delT;
        double maxStep = delT;
        double delTActual;
        return this->ComputeNextStep(xprev, nullptr, xnext, t, delT, delTActual,
                                     minStep, maxStep, maxError, error);
    }

    int ComputeNextStep(double *xprev, double *dxprev, double *xnext, double t,
                        double &delT, double maxError, double &error) override
    {
        double minStep = delT;
        double maxStep = delT;
        double delTActual;
        return this->ComputeNextStep(xprev, dxprev, xnext, t, delT, delTActual,
                                     minStep, maxStep, maxError, error);
    }

    int ComputeNextStep(double *xprev, double *xnext, double t, double &delT,
                        double &delTActual, double minStep, double maxStep,
                        double maxError, double &error) override
    {
        return this->ComputeNextStep(xprev, nullptr, xnext, t, delT, delTActual,
                                     minStep, maxStep, maxError, error);
    }

    // Advances one fixed step of length delT. minStep, maxStep and maxError are
    // accepted for interface compatibility and ignored: this solver is not
    // adaptive, so it always takes the step it is given. On success delTActual
    // equals delT; when a stage leaves the domain it reports how far the
    // integration actually got, following the vtkRungeKutta4 convention.
    int ComputeNextStep(double *xprev, double *dxprev, double *xnext, double t,
                        double &delT, double &delTActual, double minStep,
                        double maxStep, double maxError, double &error) override;

protected:
    RungeKutta5();
    ~RungeKutta5() override;

    void Initialize() override;

    // Stage derivatives k2..k6. k1 lives in the base class Derivs array.
    double *NextDerivs[5];

private:
    RungeKutta5(const RungeKutta5 &) = delete;
    void operator=(const RungeKutta5 &) = delete;
};

// vtkRungeKutta45 with the degenerate bounds that vtkParticleTracerBase passes
// repaired. IntegrateParticle declares `double minStep = 0, maxStep = 0;`
// (vtkParticleTracerBase.cxx:1065) and forwards them unchanged, while
// vtkRungeKutta45 uses minStep as the step to take when the embedded error
// estimate is exactly zero (vtkRungeKutta45.cxx:145-148:
// `tmp = delT < 0 ? -minStep : minStep`). A zero delT then trips the underflow
// guard at vtkRungeKutta45.cxx:177-184 and every call returns
// UNEXPECTED_VALUE, so the particle tracer never advances a particle at all.
// Repairing a non-positive bound to a fraction of the requested step keeps the
// adaptive controller working. The steady streamline kernel passes real bounds
// (StreamlineParameters::minimumStep/maximumStep), so there this subclass
// behaves exactly like vtkRungeKutta45.
class BoundedRungeKutta45 : public vtkRungeKutta45
{
public:
    vtkTypeMacro(BoundedRungeKutta45, vtkRungeKutta45);
    void PrintSelf(ostream &os, vtkIndent indent) override;

    static BoundedRungeKutta45 *New();

    int ComputeNextStep(double *xprev, double *dxprev, double *xnext, double t,
                        double &delT, double &delTActual, double minStep,
                        double maxStep, double maxError, double &error) override;

protected:
    BoundedRungeKutta45();
    ~BoundedRungeKutta45() override;

private:
    BoundedRungeKutta45(const BoundedRungeKutta45 &) = delete;
    void operator=(const BoundedRungeKutta45 &) = delete;
};

// Integrator codes shared by the steady ⑦ and unsteady ⑧ parameters.
enum IntegratorCode
{
    IntegratorRungeKutta2 = 0,
    IntegratorRungeKutta4 = 1,
    IntegratorRungeKutta45 = 2,
    IntegratorRungeKutta5 = 3
};

// Builds the solver for a code, or a null pointer for an unknown one. Callers
// should hand the result to SetIntegrator rather than SetIntegratorType: the
// tracers only recognise codes 0..2 and merely warn, keeping the previous
// solver, for anything else.
vtkSmartPointer<vtkInitialValueProblemSolver> createIntegrator(int code);

// Name used in messages and tests; "unknown" for an out-of-range code.
const char *integratorName(int code);

} // namespace visualization
} // namespace aerosim

#endif
