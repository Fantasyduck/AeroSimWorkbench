#pragma once

#include "aerosim/ui/monitor/MonitorCurveWidget.h"

#include <QHash>
#include <QPointF>
#include <QStringList>
#include <QVector>

namespace aerosim {

namespace monitor {
class MonitorChartView;
}

// Bridges the two monitor models in this repository.
//
// The task services speak MonitorSampleV2: one metric at a time, one value per
// delivery (contracts/include/aerosim/contracts/Telemetry.h). The chart widget
// speaks MonitorCurve: one whole series with all of its samples at once. Nothing
// in either layer should learn about the other -- the task side must not depend
// on a plotting library, and the plotting side must not depend on task
// scheduling -- so the translation lives in this adapter and nowhere else.
//
// The adapter keeps one series per metric key *and* component. A solver that
// reports CD and, separately, the upper and lower surface breakdown of it sends
// three series under one metric key; keying on the metric alone would splice
// them into a single curve that alternates between three unrelated values.
//
// Series whose quantity reads as a residual -- either the contract says so or
// the metric name does -- share the left axis, everything else shares the right
// one, which is the grouping the chart widget supports (two axes, one per side).
// A task that reports only coefficients therefore gets a single axis, and a task
// that reports both gets the split that keeps residuals from flattening
// coefficients.
//
// Live deliveries are appended without rebuilding the drawing, so a solver that
// reports a sample per iteration costs one point per sample instead of a full
// re-plot. A delivery that introduces a series nobody has seen before does need
// a rebuild: the drawing has no curve to append to.
class MonitorSampleCurveWidget final : public MonitorCurveWidget {
    Q_OBJECT
public:
    explicit MonitorSampleCurveWidget(QWidget *parent = nullptr);
    ~MonitorSampleCurveWidget() override;

    // Replaces the whole drawing. An empty taskId clears it, which is what the
    // page passes when a batch rather than a single task is selected.
    void replaceTask(const QString &taskId, const QString &attemptId,
                     const QVector<MonitorSampleV2> &history) override;
    // Appends one live sample. Ignored for another task's samples, and ignored
    // when the value is not finite.
    void appendSample(const MonitorSampleV2 &sample) override;

    // Read-back for tests and for a panel that wants to describe the drawing.
    QString taskId() const { return taskId_; }
    QString attemptId() const { return attemptId_; }
    // Series labels in the order they were first seen. A label is the metric key
    // alone when the contract carries no component, and "metric/component"
    // otherwise, so two series of one metric stay distinguishable.
    QStringList metricKeys() const { return seriesOrder_; }
    // Samples retained for one series label; 0 for a label never seen.
    int pointCount(const QString &metric) const;
    // The drawing this adapter drives, for callers that own style and scope
    // edits. Never null.
    monitor::MonitorChartView *chartView() const { return chart_; }

    // Label under which samples of one contract sample are grouped.
    static QString seriesLabelFor(const MonitorSampleV2 &sample);

private:
    // Drops every retained sample and starts a new identity.
    void resetState(const QString &taskId, const QString &attemptId);
    // Records one sample under its series label and returns the index of it.
    int ingest(const MonitorSampleV2 &sample);
    // Replaces the drawing with one series per known label.
    void rebuildCurves();
    QString curveIdFor(const QString &label) const;
    // 0 for residuals, 1 for everything else. Stable across rebuilds so a
    // series never jumps from one axis to the other.
    static int groupIndexFor(const MonitorSampleV2 &sample, const QString &label);
    static QString groupKeyFor(int groupIndex);
    static QString groupTitleFor(int groupIndex);

    monitor::MonitorChartView *chart_ = nullptr;
    QString taskId_;
    QString attemptId_;
    // Identity used for curve ids. Attempt-scoped, so a retried run produces new
    // curves instead of silently overwriting the previous attempt's.
    QString datasetId_;
    QStringList seriesOrder_;
    QHash<QString, QVector<QPointF>> pointsByMetric_;
    // Axis group decided when the series was first seen. The contract quantity
    // arrives on every sample, but rebuildCurves only has the retained points,
    // and re-deriving the group from the name alone would drop a residual whose
    // name the solver chose freely onto the coefficient axis.
    QHash<QString, int> groupByLabel_;
    // Stands in for the X coordinate when a source reports neither a solver step
    // nor a sequence number.
    QHash<QString, int> fallbackSteps_;
};

} // namespace aerosim
