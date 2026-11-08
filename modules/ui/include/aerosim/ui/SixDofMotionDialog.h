#pragma once
#include <QDialog>
#include <QVariantMap>

namespace aerosim {
class SixDofMotionDialog final : public QDialog {
public:
    enum class Mode { SixDof, RotationRelease };
    explicit SixDofMotionDialog(const QVariantMap &parameters, QWidget *parent = nullptr, Mode mode = Mode::SixDof);
    QVariantMap parameters() const { return parameters_; }
private:
    QVariantMap parameters_;
};
}
