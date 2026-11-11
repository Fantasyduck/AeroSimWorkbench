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

QString MonitorSampleCurveWidget::seriesLabelFor(const MonitorSampleV2 &sample)
{
    if (sample.component.isEmpty()) return sample.metricKey;
    return sample.metricKey + QLatin1Char('/') + sample.component;
}

MonitorSampleCurveWidget::MonitorSampleCurveWidget(QWidget *parent)
    : MonitorCurveWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    chart_ = new monitor::MonitorChartView(this);
    // The X coordinate of a streamed sample is the solver's own step number,
    // not a file iteration column, so the caption names what it actually is.
    chart_->setAxisLabel(QStringLiteral(u"求解步"));
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
    seriesOrder_.clear();
    pointsByMetric_.clear();
    groupByLabel_.clear();
    fallbackSteps_.clear();
}

void MonitorSampleCurveWidget::replaceTask(const QString &taskId, const QString &attemptId,
                                           const QVector<MonitorSampleV2> &history)
{
    resetState(taskId, attemptId);
    for (const MonitorSampleV2 &sample : history) {
        // The page already filtered its cache, but replaceTask is also called
        // with a batch's cleared history, so the same guards apply here.
        if (sample.task.taskId != taskId_) continue;
        if (!std::isfinite(sample.value) || sample.metricKey.isEmpty()) continue;
        ingest(sample);
    }
    rebuildCurves();
}

void MonitorSampleCurveWidget::appendSample(const MonitorSampleV2 &sample)
{
    if (taskId_.isEmpty() || sample.task.taskId != taskId_) return;
    if (!std::isfinite(sample.value) || sample.metricKey.isEmpty()) return;
    const QString label = seriesLabelFor(sample);
    const bool isNewSeries = !pointsByMetric_.contains(label);
    ingest(sample);
    if (isNewSeries) {
        // A series nobody has seen needs a curve to exist before it can grow.
        rebuildCurves();
        return;
    }
    const QString id = curveIdFor(label);
    const QVector<QPointF> &points = pointsByMetric_.value(label);
    if (points.isEmpty()) {
        rebuildCurves();
        return;
    }
    // The trailing point is what just arrived. Only that point is pushed, so the
    // drawing keeps its axes, its selection and every other series as they are.
    if (!chart_->appendPoints(id, QVector<QPointF>{points.last()})) rebuildCurves();
}

int MonitorSampleCurveWidget::ingest(const MonitorSampleV2 &sample)
{
    const QString label = seriesLabelFor(sample);
    if (!pointsByMetric_.contains(label)) {
        pointsByMetric_.insert(label, QVector<QPointF>());
        fallbackSteps_.insert(label, 0);
        groupByLabel_.insert(label, groupIndexFor(sample, label));
        seriesOrder_.append(label);
    }
    // The X coordinate is the solver's step when the source reports one, because
    // that is the number the run is described by. A source that reports only a
    // delivery sequence falls back to it, and one that reports neither gets a
    // running index, so no delivery ever collapses onto x = 0.
    double x = 0.0;
    if (sample.step != 0) {
        x = double(sample.step);
        fallbackSteps_[label] = int(sample.step);
    } else if (sample.sequence != 0) {
        x = double(sample.sequence);
        fallbackSteps_[label] = int(sample.sequence);
    } else {
        const int step = fallbackSteps_.value(label);
        x = double(step);
        fallbackSteps_[label] = step + 1;
    }
    pointsByMetric_[label].append(QPointF(x, sample.value));
    return seriesOrder_.indexOf(label);
}

QString MonitorSampleCurveWidget::groupKeyFor(int groupIndex)
{
    return groupIndex == 0 ? QStringLiteral("residual") : QStringLiteral("coefficient");
}

QString MonitorSampleCurveWidget::groupTitleFor(int groupIndex)
{
    return groupIndex == 0 ? QStringLiteral(u"残差") : QStringLiteral(u"系数");
}

int MonitorSampleCurveWidget::groupIndexFor(const MonitorSampleV2 &sample, const QString &label)
{
    // The contract states the quantity outright, which is more reliable than
    // guessing from the name: a solver is free to call its residual anything.
    // The name check stays as the fallback for sources that leave the quantity
    // at Other.
    if (sample.quantity == MonitorQuantity::Residual) return 0;
    if (sample.quantity != MonitorQuantity::Other) return 1;
    return looksLikeResidual(label) ? 0 : 1;
}

void MonitorSampleCurveWidget::rebuildCurves()
{
    if (taskId_.isEmpty()) {
        chart_->clearCurves();
        chart_->setChartTitle(QString());
        return;
    }
    QVector<monitor::MonitorCurve> curves;
    curves.reserve(seriesOrder_.size());
    for (int i = 0; i < seriesOrder_.size(); ++i) {
        const QString &label = seriesOrder_.at(i);
        monitor::MonitorCurve curve;
        curve.key = label;
        curve.label = label;
        curve.datasetId = datasetId_;
        curve.points = pointsByMetric_.value(label);
        curve.color = kPalette[i % kPaletteSize];
        curve.pointSize = kPointSize;
        curve.pointShape = monitor::MonitorPointShape::Circle;
        // Dots, not a polyline: consecutive samples are separate measurements,
        // and a straight segment between them asserts continuity the stream
        // does not guarantee. The user can turn joining on in the panel.
        curve.interpolation = monitor::MonitorInterpolation::None;
        const int group = groupByLabel_.value(label, looksLikeResidual(label) ? 0 : 1);
        curve.groupKey = groupKeyFor(group);
        curve.groupTitle = groupTitleFor(group);
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

QString MonitorSampleCurveWidget::curveIdFor(const QString &label) const
{
    return monitor::MonitorChartView::makeCurveId(datasetId_, label);
}

int MonitorSampleCurveWidget::pointCount(const QString &metric) const
{
    return pointsByMetric_.value(metric).size();
}

} // namespace aerosim
