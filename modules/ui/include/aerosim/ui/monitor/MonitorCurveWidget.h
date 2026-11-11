#pragma once

#include "aerosim/contracts/Telemetry.h"
#include <QVector>
#include <QWidget>

namespace aerosim {

// Module-local adapter for the curve owner's widget. No plotting algorithm here.
// GUI-thread calls only. replaceTask must clear the previous task, even if the
// new ID is empty (batch/empty selection). appendSample is for the active task.
//
// The samples are MonitorSampleV2, not the removed MonitorSample: the contract
// now carries typed task identity (TaskRef task), the attempt, the solver step
// and the monitored quantity beside the value, so this interface passes the
// contract type through instead of a narrower copy the caller would have to
// rebuild from it.
class MonitorCurveWidget : public QWidget {
public:
    using QWidget::QWidget;
    virtual void replaceTask(const QString &taskId, const QString &attemptId,
                             const QVector<MonitorSampleV2> &history) = 0;
    virtual void appendSample(const MonitorSampleV2 &sample) = 0;
};

} // namespace aerosim
