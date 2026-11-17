#pragma once
#include <QVariantMap>
#include <QWidget>
namespace aerosim {
class OutputControlPanel final : public QWidget {
    Q_OBJECT
public:
    explicit OutputControlPanel(const QVariantMap &draft, QWidget *parent = nullptr);
signals:
    void draftChanged(const QVariantMap &draft);
private:
    QVariantMap draft_;
};
}
