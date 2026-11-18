#pragma once

#include "aerosim/ui/intelligent/ResultDataSource.h"
#include "MonitorLogAccumulator.h"
#include <memory>

class QLabel;
class QListWidget;
class QTimer;
class QPushButton;

namespace aerosim {
namespace monitor { class MonitorFileTailSource; }
class QwtResultCurveView;

// Each calculation page owns its file subscription and variable selection.
class CalculationCurveMonitor final : public QWidget {
    Q_OBJECT
public:
    explicit CalculationCurveMonitor(QWidget *parent = nullptr);
    ~CalculationCurveMonitor() override;
    bool monitorFile(const QString &path, QString *error);
    void setPaused(bool paused);
    bool poll(QString *error);
    const ResultTable &table() const { return table_; }
    QwtResultCurveView *curveView() const { return curve_; }

private:
    void updateCurves();
    void refreshView();
    std::unique_ptr<monitor::MonitorFileTailSource> source_;
    monitor::MonitorLogAccumulator accumulator_;
    ResultTable table_;
    QTimer *timer_ = nullptr;
    QLabel *path_ = nullptr;
    QLabel *status_ = nullptr;
    QListWidget *variables_ = nullptr;
    QPushButton *pause_ = nullptr;
    QwtResultCurveView *curve_ = nullptr;
};
} // namespace aerosim
