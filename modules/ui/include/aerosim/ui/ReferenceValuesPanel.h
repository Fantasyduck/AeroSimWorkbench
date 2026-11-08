#pragma once

#include <QVariantMap>
#include <QMap>
#include <QPair>
#include <QWidget>

class QComboBox;
class QFormLayout;

namespace aerosim {

// Window-local reference settings; boundary flow data is not yet supplied by a service.
class ReferenceValuesPanel final : public QWidget {
    Q_OBJECT
public:
    explicit ReferenceValuesPanel(const QVariantMap &draft, QWidget *parent = nullptr);

signals:
    void draftChanged(const QVariantMap &draft);

private:
    void updateVisibility();
    void publish();
    QVariantMap draft_;
    QComboBox *inputMode_ = nullptr;
    QComboBox *parameterType_ = nullptr;
    QWidget *boundarySource_ = nullptr;
    QFormLayout *flowForm_ = nullptr;
    QMap<QString, QPair<QWidget *, QWidget *>> conditionalRows_;
};

} // namespace aerosim
