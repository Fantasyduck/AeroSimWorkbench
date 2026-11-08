#pragma once

#include <QVariantMap>
#include <QWidget>

namespace aerosim {

// Window-local solver settings draft; does not start processes or select hardware.
class CalculationControlPanel final : public QWidget {
    Q_OBJECT
public:
    explicit CalculationControlPanel(const QVariantMap &draft, QWidget *parent = nullptr);

signals:
    void draftChanged(const QVariantMap &draft);

private:
    QVariantMap draft_;
};

} // namespace aerosim
