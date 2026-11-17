#pragma once

#include "aerosim/ui/intelligent/CsvResultReader.h"

#include <QPointer>
#include <QWidget>
#include <atomic>
#include <memory>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableWidget;
template<class T> class QFutureWatcher;

namespace aerosim {
namespace machine_learning { struct ForecastResult; }

class ResultForecastPanel final : public QWidget {
    Q_OBJECT
public:
    explicit ResultForecastPanel(QWidget *parent = nullptr);
    ~ResultForecastPanel() override;

    void setDatasets(const QVector<CsvResultTable> &datasets);
    // The page reparents this widget into its data/results splitter.
    QWidget *resultsWidget() const { return results_.data(); }

signals:
    void forecastFinished(bool success);

private:
    void refreshDataset();
    void refreshCoordinate();
    void invalidateResult();
    void updateRunButton();
    QString samplingError() const;
    void runForecast();
    void showResult(const machine_learning::ForecastResult &result, const QString &coordinate,
                    const QString &target);

    QVector<CsvResultTable> datasets_;
    QWidget *settings_ = nullptr;
    QComboBox *dataset_ = nullptr;
    QComboBox *method_ = nullptr;
    QComboBox *coordinate_ = nullptr;
    QComboBox *target_ = nullptr;
    QListWidget *inputs_ = nullptr;
    QSpinBox *window_ = nullptr;
    QSpinBox *horizon_ = nullptr;
    QDoubleSpinBox *step_ = nullptr;
    QLineEdit *gamma_ = nullptr;
    QLineEdit *regularization_ = nullptr;
    QPushButton *run_ = nullptr;
    QPushButton *cancel_ = nullptr;
    QLabel *status_ = nullptr;
    QPointer<QWidget> results_;
    QTableWidget *future_ = nullptr;
    QTableWidget *validation_ = nullptr;
    QFutureWatcher<machine_learning::ForecastResult> *watcher_ = nullptr;
    std::shared_ptr<std::atomic_bool> cancellation_;
};

} // namespace aerosim
