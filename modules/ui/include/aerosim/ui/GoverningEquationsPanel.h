#pragma once

#include <QVariantMap>
#include <QWidget>

namespace aerosim {

// Window-local configuration draft; solver validation belongs to services.
class GoverningEquationsPanel final : public QWidget {
    Q_OBJECT
public:
    explicit GoverningEquationsPanel(const QVariantMap &draft, QWidget *parent = nullptr);

signals:
    void draftChanged(const QVariantMap &draft);

private:
    void updateDraft(const QString &key, const QVariant &value);
    void refreshControls();
    QVariantMap draft_;
    QWidget *viscousFlowChoices_ = nullptr;
    QWidget *inviscidNote_ = nullptr;
    QWidget *turbulenceSettings_ = nullptr;
    QWidget *rarefiedSettings_ = nullptr;
};

} // namespace aerosim
