#include "MonitorCurveFit.h"

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

// Solved by Gaussian elimination with partial pivoting on the normal equations,
// after the x values are centred and scaled into [-1, 1]. Without that
// normalisation a fit over iteration numbers in the hundreds loses more
// precision to the normal equations than the fit is worth.
bool solveNormalEquations(QVector<QVector<double>> matrix, QVector<double> rhs,
                          QVector<double> *solution)
{
    const int n = rhs.size();
    for (int col = 0; col < n; ++col) {
        int pivot = col;
        for (int row = col + 1; row < n; ++row)
            if (std::fabs(matrix.at(row).at(col)) > std::fabs(matrix.at(pivot).at(col)))
                pivot = row;
        if (std::fabs(matrix.at(pivot).at(col)) < 1e-12)
            return false;   // singular: the request cannot be fitted
        std::swap(matrix[col], matrix[pivot]);
        std::swap(rhs[col], rhs[pivot]);

        const double diagonal = matrix.at(col).at(col);
        for (int k = col; k < n; ++k)
            matrix[col][k] /= diagonal;
        rhs[col] /= diagonal;

        for (int row = 0; row < n; ++row) {
            if (row == col)
                continue;
            const double factor = matrix.at(row).at(col);
            if (factor == 0.0)
                continue;
            for (int k = col; k < n; ++k)
                matrix[row][k] -= factor * matrix.at(col).at(k);
            rhs[row] -= factor * rhs[col];
        }
    }
    *solution = rhs;
    return true;
}

// Least-squares polynomial of the requested degree over the samples.
bool fitPolynomial(const QVector<QPointF> &samples, int degree,
                   QVector<QPointF> *out, double *rSquared, QString *note)
{
    const int n = samples.size();
    if (n < kMinSamples) {
        *note = QStringLiteral("采样点少于 %1 个，无法拟合").arg(kMinSamples);
        return false;
    }

    // Centre and scale x so the normal equations stay well conditioned.
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

    const int terms = effective + 1;
    QVector<QVector<double>> matrix(terms, QVector<double>(terms, 0.0));
    QVector<double> rhs(terms, 0.0);
    for (const QPointF &p : samples) {
        const double t = normalisedX(p.x());
        QVector<double> powers(2 * terms, 1.0);
        for (int k = 1; k < 2 * terms; ++k)
            powers[k] = powers.at(k - 1) * t;
        for (int row = 0; row < terms; ++row) {
            for (int col = 0; col < terms; ++col)
                matrix[row][col] += powers.at(row + col);
            rhs[row] += powers.at(row) * p.y();
        }
    }

    QVector<double> coefficients;
    if (!solveNormalEquations(matrix, rhs, &coefficients)) {
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
                      QVector<QPointF> *out, double *rSquared, QString *note)
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
        ok = fitMovingAverage(usable, window, &points, &rSquared, &note);
        result.window = qBound(3, window, usable.size());
    } else {
        const int requested = method == MonitorFitMethod::Linear ? 1 : degree;
        ok = fitPolynomial(usable, requested, &points, &rSquared, &note);
        result.degree = method == MonitorFitMethod::Linear ? 1
                                                           : qBound(1, degree, usable.size() - 1);
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
