#pragma once
#include <QDialog>
#include <QVariantMap>

namespace aerosim {
class ForcedMotionDialog final : public QDialog {
public:
    explicit ForcedMotionDialog(const QVariantMap &parameters, QWidget *parent = nullptr);
    QVariantMap parameters() const { return parameters_; }
private:
    QVariantMap parameters_;
};
}
