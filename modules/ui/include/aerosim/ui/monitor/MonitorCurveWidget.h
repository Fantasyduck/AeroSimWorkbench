#pragma once

#include "aerosim/contracts/Telemetry.h"
#include <QVector>
#include <QWidget>

namespace aerosim {

// Module-local adapter for the curve owner's widget. No plotting algorithm here.
// GUI-thread calls only. replaceTask must clear the previous task, even if the
// new ID is empty (batch/empty selection). appendSample is for the active task.
class MonitorCurveWidget : public QWidget {
public:
    using QWidget::QWidget;
    virtual void replaceTask(const QString &taskId, const QString &attemptId,
                             const QVector<MonitorSample> &history) = 0;
    virtual void appendSample(const MonitorSample &sample) = 0;
};

} // namespace aerosim
