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
// The task services speak MonitorSample: one metric at a time, one value per
// delivery (contracts/include/aerosim/contracts/Telemetry.h). The chart widget
// speaks MonitorCurve: one whole series with all of its samples at once. Nothing
// in either layer should learn about the other -- the task side must not depend
// on a plotting library, and the plotting side must not depend on task
// scheduling -- so the translation lives in this adapter and nowhere else.
//
// The adapter keeps one series per metric name. Metrics whose names read as
// residuals share the left axis, everything else shares the right one, which is
// the grouping the chart widget supports (two axes, one per side). A task that
// reports only coefficients therefore gets a single axis, and a task that
// reports both gets the split that keeps residuals from flattening coefficients.
//
// Live deliveries are appended without rebuilding the drawing, so a solver that
// reports a sample per iteration costs one point per sample instead of a full
// re-plot. A delivery that introduces a metric nobody has seen before does need
// a rebuild: the drawing has no curve to append to.
class MonitorSampleCurveWidget final : public MonitorCurveWidget {
    Q_OBJECT
public:
    explicit MonitorSampleCurveWidget(QWidget *parent = nullptr);
    ~MonitorSampleCurveWidget() override;

    // Replaces the whole drawing. An empty taskId clears it, which is what the
    // page passes when a batch rather than a single task is selected.
    void replaceTask(const QString &taskId, const QString &attemptId,
                     const QVector<MonitorSample> &history) override;
    // Appends one live sample. Ignored for another task's samples, and ignored
    // when the value is not finite.
    void appendSample(const MonitorSample &sample) override;

    // Read-back for tests and for a panel that wants to describe the drawing.
    QString taskId() const { return taskId_; }
    QString attemptId() const { return attemptId_; }
    // Metric names in the order their series were first seen.
    QStringList metricKeys() const { return metricOrder_; }
    // Samples retained for one metric; 0 for a metric that never arrived.
    int pointCount(const QString &metric) const;
    // The drawing this adapter drives, for callers that own style and scope
    // edits. Never null.
    monitor::MonitorChartView *chartView() const { return chart_; }

private:
    // Drops every retained sample and starts a new identity.
    void resetState(const QString &taskId, const QString &attemptId);
    // Records one sample under its metric and returns the index of that metric.
    int ingest(const MonitorSample &sample);
    // Replaces the drawing with one series per known metric.
    void rebuildCurves();
    QString curveIdFor(const QString &metric) const;
    // 0 for residuals, 1 for everything else. Stable across rebuilds so a
    // series never jumps from one axis to the other.
    static int groupIndexFor(const QString &metric);
    static QString groupKeyFor(int groupIndex);
    static QString groupTitleFor(int groupIndex);

    monitor::MonitorChartView *chart_ = nullptr;
    QString taskId_;
    QString attemptId_;
    // Identity used for curve ids. Attempt-scoped, so a retried run produces new
    // curves instead of silently overwriting the previous attempt's.
    QString datasetId_;
    QStringList metricOrder_;
    QHash<QString, QVector<QPointF>> pointsByMetric_;
    // Stands in for the X coordinate when a source reports no sequence number.
    QHash<QString, int> fallbackSteps_;
};

} // namespace aerosim
