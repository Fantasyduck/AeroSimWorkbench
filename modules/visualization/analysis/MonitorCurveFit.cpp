#include "MonitorCurveFit.h"

#include "MonitorEigen.h"

#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace aerosim {
namespace monitor {
namespace {

// Number of points a fit is resampled to. A fit is a smooth function, so the
// drawing only needs enough points to look smooth at any zoom a monitor chart
// reaches; keeping it modest also keeps the render cheap.
constexpr int kFitSamples = 240;
// Fewest samples any fit is attempted from. A straight line through one point is
// not a trend, and a polynomial through two points says nothing the samples did
// not already say.
constexpr int kMinSamples = 3;

// Least squares by column-pivoted QR of the design matrix itself.
//
// This used to form the normal equations A^T A x = A^T b and solve them with
// Gaussian elimination. That squares the condition number, which is exactly the
// wrong thing for a Vandermonde design: at degree 9 over samples whose x runs
// into the hundreds the columns are nearly dependent, so the normal equations
// spend roughly twice the digits the fit can afford. Factorising A directly
// loses only what the factorisation itself costs.
//
// The x values are still centred and scaled into [-1, 1] before the matrix is
// built. That is not what makes it stable any more, but it keeps the pivot
// thresholds meaningful and costs nothing.
bool solveLeastSquares(const QVector<QVector<double>> &design, const QVector<double> &rhs,
                       QVector<double> *solution)
{
    const int rows = design.size();
    if (rows == 0 || rhs.size() != rows)
        return false;
    const int terms = design.first().size();
    if (terms == 0)
        return false;

    Eigen::MatrixXd matrix(rows, terms);
    Eigen::VectorXd targets(rows);
    for (int row = 0; row < rows; ++row) {
        if (design.at(row).size() != terms)
            return false;
        for (int col = 0; col < terms; ++col)
            matrix(row, col) = design.at(row).at(col);
        targets(row) = rhs.at(row);
    }

    Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(matrix);
    // A rank-deficient design means the request asks for more degrees of freedom
    // than the samples can tell apart. Refusing beats drawing noise as a trend.
    if (qr.rank() < terms)
        return false;

    const Eigen::VectorXd solved = qr.solve(targets);
    solution->resize(terms);
    for (int i = 0; i < terms; ++i) {
        if (!std::isfinite(solved(i)))
            return false;
        (*solution)[i] = solved(i);
    }
    return true;
}

// Least-squares polynomial of the requested degree over the samples.
bool fitPolynomial(const QVector<QPointF> &samples, int degree,
                   QVector<QPointF> *out, double *rSquared, QString *note,
                   int *effectiveDegree)
{
    const int n = samples.size();
    if (n < kMinSamples) {
        *note = QStringLiteral("采样点少于 %1 个，无法拟合").arg(kMinSamples);
        return false;
    }

    // Centre and scale x so the design matrix columns stay comparable; without
    // it the higher powers underflow to nothing next to the constant column.
    double xMean = 0.0;
    for (const QPointF &p : samples)
        xMean += p.x();
    xMean /= n;
    double xScale = 0.0;
    for (const QPointF &p : samples)
        xScale = qMax(xScale, std::fabs(p.x() - xMean));
    if (xScale <= 0.0)
        xScale = 1.0;

    const auto normalisedX = [xMean, xScale](double x) { return (x - xMean) / xScale; };

    // A degree at least one below the sample count, otherwise the system is
    // underdetermined and would report a perfect fit by construction.
    int effective = qBound(1, degree, qMin(MonitorCurveFit::maxDegree(), n - 1));
    if (effective != degree)
        *note = QStringLiteral("阶数已从 %1 调整为 %2").arg(degree).arg(effective);
    // Reported back so the caller can name the degree that was actually fitted.
    // It used to re-derive this from the request and forget maxDegree(), so a
    // request for 20 said "20" next to a 9th degree curve.
    if (effectiveDegree)
        *effectiveDegree = effective;

    const int terms = effective + 1;
    // One row per sample, one column per power: the design matrix the QR
    // factorisation works on directly.
    QVector<QVector<double>> design(n, QVector<double>(terms, 0.0));
    QVector<double> rhs(n, 0.0);
    for (int row = 0; row < n; ++row) {
        const double t = normalisedX(samples.at(row).x());
        double power = 1.0;
        for (int col = 0; col < terms; ++col) {
            design[row][col] = power;
            power *= t;
        }
        rhs[row] = samples.at(row).y();
    }

    QVector<double> coefficients;
    if (!solveLeastSquares(design, rhs, &coefficients)) {
        *note = QStringLiteral("方程奇异，无法拟合 %1 阶多项式").arg(effective);
        return false;
    }

    const auto evaluate = [&coefficients](double t) {
        double value = 0.0;
        for (int k = coefficients.size() - 1; k >= 0; --k)
            value = value * t + coefficients.at(k);
        return value;
    };

    double yMean = 0.0;
    for (const QPointF &p : samples)
        yMean += p.y();
    yMean /= n;
    double totalVariance = 0.0;
    double residual = 0.0;
    for (const QPointF &p : samples) {
        const double predicted = evaluate(normalisedX(p.x()));
        residual += (p.y() - predicted) * (p.y() - predicted);
        totalVariance += (p.y() - yMean) * (p.y() - yMean);
    }
    if (totalVariance <= 0.0) {
        // A perfectly flat series: the mean is the fit, and saying so is more
        // useful than dividing by zero to claim a perfect score.
        *rSquared = residual <= 0.0 ? 1.0 : 0.0;
    } else {
        *rSquared = 1.0 - residual / totalVariance;
    }

    // Resample across the measured range only -- never beyond it.
    const double xMin = samples.first().x();
    const double xMax = samples.last().x();
    out->clear();
    out->reserve(kFitSamples);
    for (int i = 0; i < kFitSamples; ++i) {
        const double x = xMin + (xMax - xMin) * i / double(kFitSamples - 1);
        out->append(QPointF(x, evaluate(normalisedX(x))));
    }
    return true;
}

// Centred moving average. Unlike a polynomial this makes no assumption about the
// shape of the trend; it only follows it, which is what a solver history with a
// noisy start actually needs.
bool fitMovingAverage(const QVector<QPointF> &samples, int window,
                      QVector<QPointF> *out, double *rSquared, QString *note,
                      int *effectiveWindow)
{
    const int n = samples.size();
    if (n < kMinSamples) {
        *note = QStringLiteral("采样点少于 %1 个，无法平滑").arg(kMinSamples);
        return false;
    }
    int effective = qBound(3, window, n);
    if (effective % 2 == 0)
        --effective;   // a centred window needs an odd width
    effective = qMax(3, effective);
    if (effective != window)
        *note = QStringLiteral("窗口已从 %1 调整为 %2").arg(window).arg(effective);
    // Reported back including the odd-width correction, which the note mentions
    // but the caller cannot reproduce on its own.
    if (effectiveWindow)
        *effectiveWindow = effective;

    const int half = effective / 2;
    QVector<double> smoothed(n, 0.0);
    for (int i = 0; i < n; ++i) {
        // The window shrinks at both ends instead of being dropped, so the fit
        // still covers the measured range from the first sample to the last.
        const int from = qMax(0, i - half);
        const int to = qMin(n - 1, i + half);
        double sum = 0.0;
        for (int k = from; k <= to; ++k)
            sum += samples.at(k).y();
        smoothed[i] = sum / double(to - from + 1);
    }

    double yMean = 0.0;
    for (const QPointF &p : samples)
        yMean += p.y();
    yMean /= n;
    double totalVariance = 0.0;
    double residual = 0.0;
    for (int i = 0; i < n; ++i) {
        const double delta = samples.at(i).y() - smoothed.at(i);
        residual += delta * delta;
        const double fromMean = samples.at(i).y() - yMean;
        totalVariance += fromMean * fromMean;
    }
    *rSquared = totalVariance <= 0.0 ? (residual <= 0.0 ? 1.0 : 0.0)
                                     : 1.0 - residual / totalVariance;

    out->clear();
    out->reserve(n);
    for (int i = 0; i < n; ++i)
        out->append(QPointF(samples.at(i).x(), smoothed.at(i)));
    return true;
}

} // namespace

MonitorFitResult MonitorCurveFit::fit(const QVector<QPointF> &samples,
                                      MonitorFitMethod method,
                                      int degree,
                                      int window)
{
    MonitorFitResult result;
    if (method == MonitorFitMethod::None) {
        result.ok = false;
        return result;
    }

    QVector<QPointF> usable;
    usable.reserve(samples.size());
    for (const QPointF &p : samples) {
        if (std::isfinite(p.x()) && std::isfinite(p.y()))
            usable.append(p);
    }
    // The fit is a function of x, so the samples have to be in x order. A history
    // file is normally already ordered, but the model never promises it.
    std::sort(usable.begin(), usable.end(),
              [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });

    if (usable.size() < kMinSamples) {
        result.note = QStringLiteral("采样点少于 %1 个，无法拟合").arg(kMinSamples);
        return result;
    }
    if (usable.first().x() == usable.last().x()) {
        result.note = QStringLiteral("横轴取值全部相同，无法拟合");
        return result;
    }

    QVector<QPointF> points;
    double rSquared = 0.0;
    QString note;
    bool ok = false;
    if (method == MonitorFitMethod::MovingAverage) {
        int applied = window;
        ok = fitMovingAverage(usable, window, &points, &rSquared, &note, &applied);
        // The value that was used, not the value that was asked for: these
        // fields are read back by the demo status bar and by any caller that
        // writes down what produced a curve.
        result.window = applied;
    } else {
        const int requested = method == MonitorFitMethod::Linear ? 1 : degree;
        int applied = requested;
        ok = fitPolynomial(usable, requested, &points, &rSquared, &note, &applied);
        result.degree = applied;
    }

    result.ok = ok;
    result.note = note;
    result.rSquared = rSquared;
    result.points = points;
    return result;
}

QString MonitorCurveFit::methodName(MonitorFitMethod method)
{
    switch (method) {
    case MonitorFitMethod::None:
        return QStringLiteral("不拟合");
    case MonitorFitMethod::Linear:
        return QStringLiteral("最小二乘直线");
    case MonitorFitMethod::Polynomial:
        return QStringLiteral("多项式最小二乘");
    case MonitorFitMethod::MovingAverage:
        return QStringLiteral("移动平均");
    }
    return QStringLiteral("不拟合");
}

} // namespace monitor
} // namespace aerosim
