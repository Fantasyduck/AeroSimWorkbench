#pragma once
#include <QVariantMap>
#include <QWidget>
class QListWidget;
namespace aerosim {
class OutputControlPanel final : public QWidget {
    Q_OBJECT
public:
    explicit OutputControlPanel(const QVariantMap &draft, QWidget *parent = nullptr);
signals:
    void draftChanged(const QVariantMap &draft);
private:
    QVariantMap draft_;
    QListWidget *flow_ = nullptr;
    QListWidget *wall_ = nullptr;
    QListWidget *history_ = nullptr;
    void loadList(QListWidget *, const QString &, const QStringList &, const QStringList &);
    void saveList(QListWidget *, const QString &);
};
}
