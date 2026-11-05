#include "aerosim/ui/monitor/MonitorSampleCurveWidget.h"

#include "MonitorChartView.h"

#include <QColor>
#include <QStringList>
#include <QVBoxLayout>
#include <cmath>

namespace aerosim {
namespace {

// Ten distinguishable strokes. Colour is the only cue that tells two series
// apart, so the palette is fixed rather than derived from the metric name.
const QColor kPalette[] = {
    QColor("#1f77b4"), QColor("#ff7f0e"), QColor("#2ca02c"), QColor("#d62728"),
    QColor("#9467bd"), QColor("#8c564b"), QColor("#e377c2"), QColor("#7f7f7f"),
    QColor("#bcbd22"), QColor("#17becf")
};
constexpr int kPaletteSize = int(sizeof(kPalette) / sizeof(kPalette[0]));

// One marker per sample. Anything larger turns a dense history into a solid
// band, which is the failure the chart widget's sampling interval exists to
// avoid in the first place.
constexpr qreal kPointSize = 3.0;

bool looksLikeResidual(const QString &metric)
{
    const QString lower = metric.toLower();
    return lower.contains(QStringLiteral("resid")) || lower.startsWith(QStringLiteral("rms"));
}

} // namespace

MonitorSampleCurveWidget::MonitorSampleCurveWidget(QWidget *parent)
    : MonitorCurveWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    chart_ = new monitor::MonitorChartView(this);
    // The X coordinate of a streamed sample is its sequence number, not a file
    // iteration column, so the caption names what it actually is.
    chart_->setAxisLabel(QStringLiteral(u"采样序号"));
    layout->addWidget(chart_);
}

MonitorSampleCurveWidget::~MonitorSampleCurveWidget() = default;

void MonitorSampleCurveWidget::resetState(const QString &taskId, const QString &attemptId)
{
    taskId_ = taskId;
    attemptId_ = attemptId;
    // Identity includes the attempt: a retried run must draw fresh curves
    // rather than append onto the failed attempt's.
    datasetId_ = attemptId.isEmpty()
        ? taskId
        : taskId + QStringLiteral("@") + attemptId;
    metricOrder_.clear();
    pointsByMetric_.clear();
    fallbackSteps_.clear();
}

void MonitorSampleCurveWidget::replaceTask(const QString &taskId, const QString &attemptId,
                                           const QVector<MonitorSample> &history)
{
    resetState(taskId, attemptId);
    for (const MonitorSample &sample : history) {
        // The page already filtered its cache, but replaceTask is also called
        // with a batch's cleared history, so the same guards apply here.
        if (sample.taskId != taskId_) continue;
        if (!std::isfinite(sample.value) || sample.metric.isEmpty()) continue;
        ingest(sample);
    }
    rebuildCurves();
}

void MonitorSampleCurveWidget::appendSample(const MonitorSample &sample)
{
    if (taskId_.isEmpty() || sample.taskId != taskId_) return;
    if (!std::isfinite(sample.value) || sample.metric.isEmpty()) return;
    const bool isNewMetric = !pointsByMetric_.contains(sample.metric);
    ingest(sample);
    if (isNewMetric) {
        // A metric nobody has seen needs a curve to exist before it can grow.
        rebuildCurves();
        return;
    }
    const QString id = curveIdFor(sample.metric);
    const QVector<QPointF> &points = pointsByMetric_.value(sample.metric);
    if (points.isEmpty()) {
        rebuildCurves();
        return;
    }
    // The trailing point is what just arrived. Only that point is pushed, so the
    // drawing keeps its axes, its selection and every other series as they are.
    if (!chart_->appendPoints(id, QVector<QPointF>{points.last()})) rebuildCurves();
}

int MonitorSampleCurveWidget::ingest(const MonitorSample &sample)
{
    if (!pointsByMetric_.contains(sample.metric)) {
        pointsByMetric_.insert(sample.metric, QVector<QPointF>());
        fallbackSteps_.insert(sample.metric, 0);
        metricOrder_.append(sample.metric);
    }
    // The X coordinate is the source's own sequence number when it supplies one.
    // A source that leaves it at zero gets a running index instead, per metric,
    // because each metric counts its own deliveries.
    double x = 0.0;
    if (sample.sequence != 0) {
        x = double(sample.sequence);
        fallbackSteps_[sample.metric] = int(sample.sequence);
    } else {
        const int step = fallbackSteps_.value(sample.metric);
        x = double(step);
        fallbackSteps_[sample.metric] = step + 1;
    }
    pointsByMetric_[sample.metric].append(QPointF(x, sample.value));
    return metricOrder_.indexOf(sample.metric);
}

QString MonitorSampleCurveWidget::groupKeyFor(int groupIndex)
{
    return groupIndex == 0 ? QStringLiteral("residual") : QStringLiteral("coefficient");
}

QString MonitorSampleCurveWidget::groupTitleFor(int groupIndex)
{
    return groupIndex == 0 ? QStringLiteral(u"残差") : QStringLiteral(u"系数");
}

int MonitorSampleCurveWidget::groupIndexFor(const QString &metric)
{
    return looksLikeResidual(metric) ? 0 : 1;
}

void MonitorSampleCurveWidget::rebuildCurves()
{
    if (taskId_.isEmpty()) {
        chart_->clearCurves();
        chart_->setChartTitle(QString());
        return;
    }
    QVector<monitor::MonitorCurve> curves;
    curves.reserve(metricOrder_.size());
    for (int i = 0; i < metricOrder_.size(); ++i) {
        const QString &metric = metricOrder_.at(i);
        monitor::MonitorCurve curve;
        curve.key = metric;
        curve.label = metric;
        curve.datasetId = datasetId_;
        curve.points = pointsByMetric_.value(metric);
        curve.color = kPalette[i % kPaletteSize];
        curve.pointSize = kPointSize;
        curve.pointShape = monitor::MonitorPointShape::Circle;
        // Dots, not a polyline: consecutive samples are separate measurements,
        // and a straight segment between them asserts continuity the stream
        // does not guarantee. The user can turn joining on in the panel.
        curve.interpolation = monitor::MonitorInterpolation::None;
        curve.groupKey = groupKeyFor(groupIndexFor(metric));
        curve.groupTitle = groupTitleFor(groupIndexFor(metric));
        curves.append(curve);
    }
    chart_->setCurves(curves);
    // The caption names the magnitudes on screen, not the task (the page header
    // already says which task this is) and not a count of series. Two groups on
    // one drawing get both names; one group just gets its own.
    QStringList titles;
    for (const monitor::MonitorCurve &curve : curves) {
        if (!titles.contains(curve.groupTitle)) titles.append(curve.groupTitle);
    }
    chart_->setChartTitle(titles.join(QStringLiteral(" · ")));
}

QString MonitorSampleCurveWidget::curveIdFor(const QString &metric) const
{
    return monitor::MonitorChartView::makeCurveId(datasetId_, metric);
}

int MonitorSampleCurveWidget::pointCount(const QString &metric) const
{
    return pointsByMetric_.value(metric).size();
}

} // namespace aerosim
