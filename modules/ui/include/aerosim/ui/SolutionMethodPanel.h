#pragma once
#include <QVariantMap>
#include <QWidget>
class QFormLayout;

namespace aerosim {
class SolutionMethodPanel final : public QWidget {
    Q_OBJECT
public:
    explicit SolutionMethodPanel(const QVariantMap &draft, QWidget *parent = nullptr);
    void setUnsteady(bool unsteady);
signals:
    void draftChanged(const QVariantMap &draft);
private:
    QVariantMap draft_;
    QFormLayout *form_ = nullptr;
    QList<QWidget *> unsteadyFields_;
};
}
