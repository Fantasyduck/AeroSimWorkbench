#pragma once

#include "aerosim/ui/intelligent/ResultDataSource.h"

#include "MonitorChartView.h"

#include <QPointer>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace aerosim {

// Curve half of the merged result page, drawn with the monitor chart widget.
//
// Composition rather than inheritance: the chart widget owns its own plot,
// legend, picker and axes, and the page never sees any of that -- it hands over
// keys and gets a picture back.
class QwtResultCurveView final : public ResultCurveView {
public:
    explicit QwtResultCurveView(QWidget *parent = nullptr);
    ~QwtResultCurveView() override;

    QWidget *settingsPanel() const override { return settingsPanel_.data(); }

    void replaceSeries(const ResultTable &table, const QStringList &keys,
                       const QString &axisLabel) override;
    void clearSeries() override;

    // Exposed so a test can assert on what was actually drawn rather than on the
    // settings that were requested.
    monitor::MonitorChartView *chart() const { return chart_; }

private:
    void refreshCurveChoices();
    void loadControls();
    void chooseColor();
    void applyControls();
    void selectCurve();

    QPointer<QWidget> settingsPanel_;
    QComboBox *curveChoice_ = nullptr;
    QPushButton *color_ = nullptr;
    QComboBox *lineStyle_ = nullptr;
    QDoubleSpinBox *lineWidth_ = nullptr;
    QComboBox *shape_ = nullptr;
    QDoubleSpinBox *pointSize_ = nullptr;
    QComboBox *drawingMode_ = nullptr;
    QSpinBox *fitDegree_ = nullptr;
    QSpinBox *fitWindow_ = nullptr;
    QLabel *fitFeedback_ = nullptr;
    monitor::MonitorChartView *chart_ = nullptr;
    QHash<QString, monitor::MonitorCurveStyle> styles_;
    bool loadingControls_ = false;
};

} // namespace aerosim
