#pragma once

#include <QDialog>
#include <QStringList>
#include <QVector>

class QLabel;
class QLineEdit;

namespace aerosim {

// Local input modes for the reference UI, not persisted engineering states.
enum class CalculationInputKind { ReynoldsTemperature, PressureTemperature, TotalPressureTemperature, Altitude, Trajectory };

QString calculationInputTitle(CalculationInputKind kind);
QStringList calculationInputLabels(CalculationInputKind kind);

class CalculationTaskInputDialog final : public QDialog {
    Q_OBJECT
public:
    explicit CalculationTaskInputDialog(CalculationInputKind kind, QWidget *parent = nullptr);
    QVector<QVector<double>> rows() const { return rows_; }
    void setValues(const QVector<double> &values);

private:
    void validateAndAccept();
    CalculationInputKind kind_;
    QVector<QLineEdit *> inputs_;
    QLabel *error_ = nullptr;
    QVector<QVector<double>> rows_;
    bool editing_ = false;
};

} // namespace aerosim
