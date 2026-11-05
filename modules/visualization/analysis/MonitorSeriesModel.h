#pragma once

#include "MonitorLogReader.h"

#include <QPointF>
#include <QStringList>
#include <QVector>

namespace aerosim {
namespace monitor {

// Horizontal axis of a monitor curve. Only axes backed by real file data are
// offered; see axisAvailable().
enum class MonitorAxis {
    IterationStep = 0,
    TimeIndex = 1
};

// How a series should be drawn together with others.
enum class MonitorScale {
    Linear = 0,
    Logarithmic = 1
};

// Two-dimensional view of one MonitorLog. Holds no QWidget and no VTK type, so
// the plotting decision can be tested headless and a different charting library
// can replace Qwt without touching this model.
class MonitorSeriesModel {
public:
    bool setLog(const MonitorLog &log, QString *error);

    bool isValid() const { return valid_; }
    const MonitorLog &log() const { return log_; }
    QString sourcePath() const { return log_.sourcePath; }
    QString datasetId() const { return log_.datasetId; }
    int rowCount() const { return log_.iterationStep.size(); }

    // Returns false when the axis is degenerate for this file, and explains why
    // through *reason so the workbench can grey the option out with a real cause
    // instead of leaving a silent no-op selection.
    bool axisAvailable(MonitorAxis axis) const;
    bool axisAvailable(MonitorAxis axis, QString *reason) const;
    QString axisLabel(MonitorAxis axis) const;

    QVector<QPointF> points(const QString &columnKey, MonitorAxis axis) const;

    bool isResidual(const QString &columnKey) const;
    QString unit(const QString &columnKey) const;
    MonitorScale suggestedScale(const QString &columnKey) const;
    MonitorScale suggestedScale(const QStringList &columnKeys) const;

    // Grouping keeps residuals (order 1e-8) and aerodynamic coefficients (order
    // 1e0) from sharing one axis without warning.
    QStringList residualColumns() const;
    QStringList coefficientColumns() const;
    QStringList seriesColumns() const;      // every plottable column, in file order
    QStringList warnings() const { return log_.warnings; }

private:
    bool valid_ = false;
    MonitorLog log_;
};

} // namespace monitor
} // namespace aerosim
