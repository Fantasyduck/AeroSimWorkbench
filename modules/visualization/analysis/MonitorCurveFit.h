#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

namespace aerosim {
namespace monitor {

// How a fitted curve is derived from the samples.
//
// This is deliberately a different question from MonitorInterpolation. Joining
// decides how two neighbouring samples are connected and therefore always passes
// through every one of them; a fit approximates the trend of the whole series and
// is not expected to touch a single sample. A UI needs both: the raw samples, the
// optional joining stroke, and optionally a fitted trend on top of them.
enum class MonitorFitMethod {
    None = 0,           // no fitted curve
    Linear = 1,         // least-squares straight line
    Polynomial = 2,     // least-squares polynomial of the requested degree
    MovingAverage = 3   // centred moving average of the requested width
};

// A fitted curve together with what it is worth.
//
// The goodness of fit travels with the points because a fit can be meaningless:
// a solver history is not obliged to follow any polynomial, and a high degree
// will happily produce a curve that wiggles through noise. Reporting rSquared
// lets the caller decide whether to believe it instead of the library pretending
// every fit is a discovery.
struct MonitorFitResult {
    // Densely resampled inside the x range of the samples. The fit is never
    // extrapolated: drawing a trend beyond the measured range would invent data.
    QVector<QPointF> points;
    double rSquared = 0.0;
    int degree = 0;   // effective polynomial degree, after clamping
    int window = 0;   // effective moving-average width, after clamping
    bool ok = false;
    QString note;     // why the fit failed, or what had to be reduced

    bool isValid() const { return ok && points.size() >= 2; }
};

// Fit algorithms for one monitor series.
//
// Pure arithmetic on points: no widget and no charting library, so it runs
// headless and is unit-tested on its own. The charting library has no
// least-squares fitter at all (every Qwt spline interpolates, i.e. passes through
// the samples), which is why this lives here rather than in charts/.
class MonitorCurveFit {
public:
    // Fits one series. An unavailable or degenerate request comes back with
    // ok == false and a readable note rather than a silently substituted curve.
    static MonitorFitResult fit(const QVector<QPointF> &samples,
                                MonitorFitMethod method,
                                int degree = 3,
                                int window = 7);

    // Caption for a method, e.g. for a combo box or a status line.
    static QString methodName(MonitorFitMethod method);

    // Largest polynomial the solver is asked for. Beyond this the normal
    // equations lose precision faster than the curve gains detail.
    static int maxDegree() { return 9; }
};

} // namespace monitor
} // namespace aerosim
