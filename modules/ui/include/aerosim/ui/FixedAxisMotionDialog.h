#pragma once
#include <QDialog>
#include <QVariantMap>

class QComboBox;
class QSpinBox;
class QTableWidget;
namespace aerosim {
class FixedAxisMotionDialog final : public QDialog {
public:
    explicit FixedAxisMotionDialog(const QVariantMap &parameters, QWidget *parent = nullptr);
    QVariantMap parameters() const { return parameters_; }
private:
    void storeForce();
    void loadForce(int index);
    QVariantMap parameters_;
    QVariantList forces_;
    QComboBox *force_ = nullptr;
    QComboBox *frame_ = nullptr;
    QSpinBox *points_ = nullptr;
    QTableWidget *table_ = nullptr;
    int selected_ = -1;
};
}
