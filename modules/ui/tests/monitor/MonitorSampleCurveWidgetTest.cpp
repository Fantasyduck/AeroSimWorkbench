#include "aerosim/ui/monitor/MonitorSampleCurveWidget.h"

#include "MonitorChartView.h"

#include <QApplication>
#include <QTextStream>
#include <cmath>
#include <limits>

using namespace aerosim;

namespace {

int failures = 0;

void check(bool passed, const QString &what)
{
    if (!passed) {
        QTextStream(stderr) << "FAIL: " << what << "\n";
        ++failures;
    }
}

MonitorSampleV2 sample(const QString &task, const QString &metric, quint64 sequence, double value)
{
    MonitorSampleV2 s;
    s.task.taskId = task;
    s.metricKey = metric;
    s.sequence = sequence;
    s.value = value;
    s.unit = QStringLiteral("-");
    // The contract states the quantity outright, so the test states it too
    // instead of leaning on the adapter's name fallback.
    s.quantity = metric.startsWith(QStringLiteral("rms")) ? MonitorQuantity::Residual
                                                         : MonitorQuantity::Coefficient;
    return s;
}

// The drawing a task's samples produce must be one series per metric, with the
// residual series on the left axis and everything else on the right one. That
// split is the whole reason this adapter assigns group keys at all.
void testReplaceTaskGroupsMetrics()
{
    MonitorSampleCurveWidget widget;
    QVector<MonitorSampleV2> history;
    history << sample(QStringLiteral("Run_1"), QStringLiteral("rms[Rho]"), 1, -1.5)
            << sample(QStringLiteral("Run_1"), QStringLiteral("rms[Rho]"), 2, -2.5)
            << sample(QStringLiteral("Run_1"), QStringLiteral("CD"), 1, 0.03)
            << sample(QStringLiteral("Run_1"), QStringLiteral("CD"), 2, 0.02);
    widget.replaceTask(QStringLiteral("Run_1"), QStringLiteral("attempt-1"), history);

    check(widget.taskId() == QStringLiteral("Run_1"), QStringLiteral("taskId is retained"));
    check(widget.metricKeys().size() == 2, QStringLiteral("one series per metric"));
    check(widget.pointCount(QStringLiteral("rms[Rho]")) == 2, QStringLiteral("residual points"));
    check(widget.pointCount(QStringLiteral("CD")) == 2, QStringLiteral("coefficient points"));

    auto *chart = widget.chartView();
    check(chart != nullptr, QStringLiteral("the adapter owns a chart"));
    check(chart->pointSeriesCount() == 2, QStringLiteral("two series reach the drawing"));
    // Residuals and coefficients belong to different magnitude groups, so two
    // axes; putting them on one would let the coefficient range flatten the
    // residual curve entirely.
    check(chart->axisCount() == 2, QStringLiteral("two magnitude groups get two axes"));
    // The caption names the magnitudes on screen; the page header already says
    // which task is selected, so repeating the task name here would be noise.
    check(chart->chartTitle() == QStringLiteral(u"残差 · 系数"),
          QStringLiteral("the caption names the magnitudes, not a count"));
}

// A live sample must extend the drawing in place. Rebuilding would drop the
// selection, the axes and any style the user applied, once per iteration.
void testAppendSampleDoesNotRebuild()
{
    MonitorSampleCurveWidget widget;
    QVector<MonitorSampleV2> history;
    history << sample(QStringLiteral("Run_2"), QStringLiteral("CD"), 1, 0.03);
    widget.replaceTask(QStringLiteral("Run_2"), QStringLiteral("attempt-1"), history);
    auto *chart = widget.chartView();
    const int buildsAfterReplace = chart->chartBuildCount();

    widget.appendSample(sample(QStringLiteral("Run_2"), QStringLiteral("CD"), 2, 0.02));
    check(widget.pointCount(QStringLiteral("CD")) == 2, QStringLiteral("the point is retained"));
    check(chart->chartBuildCount() == buildsAfterReplace,
          QStringLiteral("an appended sample does not rebuild the drawing"));
    check(chart->drawnSampleCount(monitor::MonitorChartView::makeCurveId(
              QStringLiteral("Run_2@attempt-1"), QStringLiteral("CD"))) == 2,
          QStringLiteral("the appended point reaches the drawing"));

    // A metric nobody has seen needs a curve to exist before it can grow, so
    // that delivery is the one case that does rebuild.
    widget.appendSample(sample(QStringLiteral("Run_2"), QStringLiteral("CL"), 2, 0.4));
    check(chart->chartBuildCount() > buildsAfterReplace,
          QStringLiteral("a brand new metric rebuilds"));
    check(widget.metricKeys().size() == 2, QStringLiteral("the new metric is registered"));
}

// Samples that belong to someone else, or that carry no usable number, must
// leave the drawing untouched rather than draw a point the solver never made.
void testRejectsForeignAndNonFiniteSamples()
{
    MonitorSampleCurveWidget widget;
    QVector<MonitorSampleV2> history;
    history << sample(QStringLiteral("Run_3"), QStringLiteral("CD"), 1, 0.03);
    widget.replaceTask(QStringLiteral("Run_3"), QStringLiteral("attempt-1"), history);
    const int before = widget.pointCount(QStringLiteral("CD"));

    widget.appendSample(sample(QStringLiteral("Run_other"), QStringLiteral("CD"), 2, 0.5));
    check(widget.pointCount(QStringLiteral("CD")) == before,
          QStringLiteral("another task's sample is ignored"));

    widget.appendSample(sample(QStringLiteral("Run_3"), QStringLiteral("CD"), 3,
                               std::numeric_limits<double>::quiet_NaN()));
    check(widget.pointCount(QStringLiteral("CD")) == before,
          QStringLiteral("a non-finite value is ignored"));

    widget.appendSample(sample(QStringLiteral("Run_3"), QString(), 4, 1.0));
    check(widget.pointCount(QString()) == 0, QStringLiteral("an empty metric is ignored"));
}

// An empty task is what the page passes when a batch, not a single task, is
// selected. The drawing must be cleared rather than left showing the previous
// task's curves.
void testEmptyTaskClears()
{
    MonitorSampleCurveWidget widget;
    QVector<MonitorSampleV2> history;
    history << sample(QStringLiteral("Run_4"), QStringLiteral("CD"), 1, 0.03);
    widget.replaceTask(QStringLiteral("Run_4"), QStringLiteral("attempt-1"), history);
    check(widget.pointCount(QStringLiteral("CD")) == 1, QStringLiteral("history drawn"));

    widget.replaceTask(QString(), QString(), {});
    check(widget.metricKeys().isEmpty(), QStringLiteral("metrics are dropped"));
    check(widget.chartView()->pointSeriesCount() == 0, QStringLiteral("the drawing is cleared"));
    check(widget.chartView()->chartTitle().isEmpty(), QStringLiteral("the caption is cleared"));
}

// A retried run is a different set of measurements. Reusing the previous
// identity would silently splice two attempts into one curve.
void testAttemptScopesIdentity()
{
    MonitorSampleCurveWidget widget;
    QVector<MonitorSampleV2> history;
    history << sample(QStringLiteral("Run_5"), QStringLiteral("CD"), 1, 0.03);
    widget.replaceTask(QStringLiteral("Run_5"), QStringLiteral("attempt-1"), history);
    const QString first = monitor::MonitorChartView::makeCurveId(
        QStringLiteral("Run_5@attempt-1"), QStringLiteral("CD"));
    check(widget.chartView()->curveIds().contains(first),
          QStringLiteral("the curve id carries the attempt"));

    widget.replaceTask(QStringLiteral("Run_5"), QStringLiteral("attempt-2"), history);
    const QString second = monitor::MonitorChartView::makeCurveId(
        QStringLiteral("Run_5@attempt-2"), QStringLiteral("CD"));
    check(widget.chartView()->curveIds().contains(second),
          QStringLiteral("a retry gets its own curves"));
    check(!widget.chartView()->curveIds().contains(first),
          QStringLiteral("the previous attempt's curves are gone"));
}

// A source that reports no sequence number still has to produce a usable X
// axis, otherwise every sample lands at x = 0 and nothing is drawn.
void testFallsBackToRunningIndex()
{
    MonitorSampleCurveWidget widget;
    QVector<MonitorSampleV2> history;
    history << sample(QStringLiteral("Run_6"), QStringLiteral("CD"), 0, 0.03)
            << sample(QStringLiteral("Run_6"), QStringLiteral("CD"), 0, 0.02);
    widget.replaceTask(QStringLiteral("Run_6"), QStringLiteral("attempt-1"), history);
    check(widget.pointCount(QStringLiteral("CD")) == 2, QStringLiteral("both samples retained"));

    const QString id = monitor::MonitorChartView::makeCurveId(
        QStringLiteral("Run_6@attempt-1"), QStringLiteral("CD"));
    const QVector<QPointF> points = widget.chartView()->curvePoints(id);
    check(points.size() == 2, QStringLiteral("both points are drawn"));
    if (points.size() == 2) {
        check(!qFuzzyCompare(points.at(0).x(), points.at(1).x()),
              QStringLiteral("the fallback index advances"));
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    testReplaceTaskGroupsMetrics();
    testAppendSampleDoesNotRebuild();
    testRejectsForeignAndNonFiniteSamples();
    testEmptyTaskClears();
    testAttemptScopesIdentity();
    testFallsBackToRunningIndex();
    if (failures != 0) {
        QTextStream(stderr) << failures << " monitor curve check(s) failed\n";
        return 1;
    }
    QTextStream(stdout) << "Monitor sample curve widget OK\n";
    return 0;
}
