#ifdef AEROSIM_FEATURES_REQUIRES_DEBUG
#error "Use Debug with the project VTK 8.2 installation"
#endif

#include "aerosim/visualization/features/RungeKutta5.h"

#include <vtkFunctionSet.h>
#include <vtkObjectFactory.h>
#include <vtkRungeKutta2.h>
#include <vtkRungeKutta4.h>
#include <vtkRungeKutta45.h>

#include <cmath>
#include <cstring>
#include <ostream>

namespace aerosim {
namespace visualization {
namespace {

// Classic six-stage fifth-order Butcher tableau.
//
//   c = 0, 1/4, 1/4, 1/2, 3/4, 1
//
// Each row lists the a[i][j] weights that build stage i from the stages before
// it, so A[0] belongs to the first stage (which only evaluates the start
// point) and A[5] to the last one.
const double kStageC[6] = {0.0, 0.25, 0.25, 0.5, 0.75, 1.0};
const double kStageA[6][5] = {
    {0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0 / 4.0, 0.0, 0.0, 0.0, 0.0},
    {1.0 / 8.0, 1.0 / 8.0, 0.0, 0.0, 0.0},
    {0.0, -1.0 / 2.0, 1.0, 0.0, 0.0},
    {3.0 / 16.0, 0.0, 0.0, 9.0 / 16.0, 0.0},
    {-3.0 / 7.0, 2.0 / 7.0, 12.0 / 7.0, -12.0 / 7.0, 8.0 / 7.0}};

// Final combination weights, in the same stage order.
const double kStageB[6] = {7.0 / 90.0, 0.0, 32.0 / 90.0, 12.0 / 90.0,
                           32.0 / 90.0, 7.0 / 90.0};

} // namespace

vtkStandardNewMacro(RungeKutta5);

RungeKutta5::RungeKutta5()
{
    for (int i = 0; i < 5; ++i)
        this->NextDerivs[i] = nullptr;
}

RungeKutta5::~RungeKutta5()
{
    for (int i = 0; i < 5; ++i)
    {
        delete[] this->NextDerivs[i];
        this->NextDerivs[i] = nullptr;
    }
}

void RungeKutta5::Initialize()
{
    // The base class allocates Vals and Derivs and sets Initialized.
    this->vtkInitialValueProblemSolver::Initialize();
    if (!this->FunctionSet || !this->Initialized)
        return;
    const int count = this->FunctionSet->GetNumberOfFunctions();
    for (int i = 0; i < 5; ++i)
    {
        delete[] this->NextDerivs[i];
        this->NextDerivs[i] = new double[count];
    }
}

int RungeKutta5::ComputeNextStep(double *xprev, double *dxprev, double *xnext,
                                 double t, double &delT, double &delTActual,
                                 double, double, double, double &error)
{
    delTActual = 0.0;
    error = 0.0;

    if (!this->FunctionSet)
    {
        vtkErrorMacro("No derivative functions are provided!");
        return NOT_INITIALIZED;
    }
    if (!this->Initialized)
    {
        vtkErrorMacro("Integrator not initialized!");
        return NOT_INITIALIZED;
    }
    // A non-positive step cannot advance anything, and honouring it would leave
    // the tracer to interpret a zero-length move as progress.
    if (delT == 0.0)
    {
        memcpy(xnext, xprev, this->FunctionSet->GetNumberOfFunctions() * sizeof(double));
        return UNEXPECTED_VALUE;
    }

    const int numDerivs = this->FunctionSet->GetNumberOfFunctions();
    const int numVals = numDerivs + 1;

    // Stage 1 evaluates the start point. The caller may pass the derivative it
    // already holds, which saves one field evaluation per step.
    for (int i = 0; i < numDerivs; ++i)
        this->Vals[i] = xprev[i];
    this->Vals[numVals - 1] = t;
    if (dxprev)
    {
        for (int i = 0; i < numDerivs; ++i)
            this->Derivs[i] = dxprev[i];
    }
    else if (!this->FunctionSet->FunctionValues(this->Vals, this->Derivs))
    {
        memcpy(xnext, this->Vals, numDerivs * sizeof(double));
        return OUT_OF_DOMAIN;
    }

    // Stages 2..6. A failure reports the time actually reached, mirroring
    // vtkRungeKutta4, so the tracer can stop at a point still inside the domain.
    for (int stage = 1; stage < 6; ++stage)
    {
        for (int i = 0; i < numDerivs; ++i)
        {
            double sum = 0.0;
            for (int j = 0; j < stage; ++j)
            {
                const double *source = (j == 0) ? this->Derivs : this->NextDerivs[j - 1];
                sum += kStageA[stage][j] * source[i];
            }
            this->Vals[i] = xprev[i] + delT * sum;
        }
        this->Vals[numVals - 1] = t + kStageC[stage] * delT;

        double *stageDerivs = this->NextDerivs[stage - 1];
        if (!this->FunctionSet->FunctionValues(this->Vals, stageDerivs))
        {
            memcpy(xnext, this->Vals, numDerivs * sizeof(double));
            delTActual = kStageC[stage] * delT;
            return OUT_OF_DOMAIN;
        }
    }

    for (int i = 0; i < numDerivs; ++i)
    {
        double sum = kStageB[0] * this->Derivs[i];
        for (int stage = 1; stage < 6; ++stage)
            sum += kStageB[stage] * this->NextDerivs[stage - 1][i];
        xnext[i] = xprev[i] + delT * sum;
    }
    delTActual = delT;
    return 0;
}

void RungeKutta5::PrintSelf(ostream &os, vtkIndent indent)
{
    this->Superclass::PrintSelf(os, indent);
    os << indent << "Runge-Kutta 5 stage derivatives:";
    for (int i = 0; i < 5; ++i)
        os << " " << this->NextDerivs[i];
    os << endl;
}

BoundedRungeKutta45::BoundedRungeKutta45() = default;
BoundedRungeKutta45::~BoundedRungeKutta45() = default;

vtkStandardNewMacro(BoundedRungeKutta45);

void BoundedRungeKutta45::PrintSelf(ostream &os, vtkIndent indent)
{
    this->Superclass::PrintSelf(os, indent);
}

int BoundedRungeKutta45::ComputeNextStep(double *xprev, double *dxprev, double *xnext,
                                         double t, double &delT, double &delTActual,
                                         double minStep, double maxStep, double maxError,
                                         double &error)
{
    // A non-positive magnitude carries no usable step; let the solver reject it
    // through its own underflow guard instead of inventing a bound.
    const double magnitude = std::fabs(delT);
    if (magnitude <= 0.0)
    {
        return this->Superclass::ComputeNextStep(xprev, dxprev, xnext, t, delT, delTActual,
                                                  minStep, maxStep, maxError, error);
    }

    // maxStep only means something when it is positive and no larger than the
    // step actually requested for this call.
    if (maxStep <= 0.0 || maxStep > magnitude)
    {
        maxStep = magnitude;
    }
    // vtkRungeKutta45 substitutes minStep for a zero error estimate, so it has to
    // be a usable positive number; scale it with the request rather than using a
    // fixed constant, which would be wrong for very small steps.
    if (minStep <= 0.0)
    {
        minStep = magnitude * 1.0e-6;
    }
    if (minStep > maxStep)
    {
        minStep = maxStep;
    }

    return this->Superclass::ComputeNextStep(xprev, dxprev, xnext, t, delT, delTActual,
                                             minStep, maxStep, maxError, error);
}

vtkSmartPointer<vtkInitialValueProblemSolver> createIntegrator(int code)
{
    // vtkInitialValueProblemSolver is abstract, so every branch hands over a
    // concrete solver. The smart pointer is constructed from the derived
    // pointer, which takes ownership of the reference New() returned.
    switch (code)
    {
    case IntegratorRungeKutta2:
        return vtkSmartPointer<vtkInitialValueProblemSolver>(vtkRungeKutta2::New());
    case IntegratorRungeKutta4:
        return vtkSmartPointer<vtkInitialValueProblemSolver>(vtkRungeKutta4::New());
    case IntegratorRungeKutta45:
        return vtkSmartPointer<vtkInitialValueProblemSolver>(BoundedRungeKutta45::New());
    case IntegratorRungeKutta5:
        return vtkSmartPointer<vtkInitialValueProblemSolver>(RungeKutta5::New());
    default:
        break;
    }
    return vtkSmartPointer<vtkInitialValueProblemSolver>();
}

const char *integratorName(int code)
{
    switch (code)
    {
    case IntegratorRungeKutta2:
        return "RK2";
    case IntegratorRungeKutta4:
        return "RK4";
    case IntegratorRungeKutta45:
        return "RK45";
    case IntegratorRungeKutta5:
        return "RK5";
    default:
        return "unknown";
    }
}

} // namespace visualization
} // namespace aerosim
