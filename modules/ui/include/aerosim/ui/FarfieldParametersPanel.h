#pragma once

#include <QMap>
#include <QPair>
#include <QVariantMap>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QFormLayout;

namespace aerosim {

class FarfieldParametersPanel final : public QWidget {
    Q_OBJECT
public:
    explicit FarfieldParametersPanel(QWidget *parent = nullptr);
    void setParameters(const QVariantMap &parameters);
    QVariantMap parameters() const;

signals:
    void parametersChanged();

private:
    void updateFields();
    QComboBox *type_ = nullptr;
    QFormLayout *form_ = nullptr;
    QVariantMap parameters_;
    QMap<QString, QDoubleSpinBox *> numbers_;
    QMap<QString, QPair<QWidget *, QWidget *>> conditionalRows_;
};

} // namespace aerosim
