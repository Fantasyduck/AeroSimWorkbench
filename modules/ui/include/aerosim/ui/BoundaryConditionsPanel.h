#pragma once

#include <QVariantMap>
#include <QMap>
#include <QPair>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QBoxLayout;
class QResizeEvent;
class QDoubleSpinBox;
class QFormLayout;

namespace aerosim {
class FarfieldParametersPanel;

// UI draft for the reference M6 boundaries, pending real mesh service integration.
class BoundaryConditionsPanel final : public QWidget {
    Q_OBJECT
public:
    explicit BoundaryConditionsPanel(const QVariantMap &draft, QWidget *parent = nullptr);

signals:
    void draftChanged(const QVariantMap &draft);

private:
    void resizeEvent(QResizeEvent *event) override;
    void loadSelection();
    void validateEdit();
    void applyEdit();
    void updateTypeFields();
    QVariantMap currentTypeParameters() const;
    QString typeLabel(const QString &type) const;
    QVariantMap draft_;
    QTreeWidget *table_ = nullptr;
    QLineEdit *name_ = nullptr;
    QComboBox *type_ = nullptr;
    QLabel *feedback_ = nullptr;
    QPushButton *apply_ = nullptr;
    FarfieldParametersPanel *farfield_ = nullptr;
    QBoxLayout *bodyLayout_ = nullptr;
    QFormLayout *editorForm_ = nullptr;
    QMap<QString, QPair<QWidget *, QDoubleSpinBox *>> typeFields_;
    QVariantMap typeParameters_;
    QString editingType_;
};

} // namespace aerosim
