#pragma once

#include <QVariantMap>
#include <QWidget>

class QCheckBox;
class QSpinBox;
class QTableWidget;

namespace aerosim {

class MeshMotionPanel final : public QWidget {
    Q_OBJECT
public:
    explicit MeshMotionPanel(const QVariantMap &draft, QWidget *parent = nullptr);

signals:
    void draftChanged(const QVariantMap &draft);

private:
    void resizeBodies(int count);
    void rebuildTable();
    void updateBody(int row, const QString &key, const QVariant &value);
    void publish();
    QVariantMap draft_;
    QVariantList bodies_;
    QCheckBox *enabled_ = nullptr;
    QSpinBox *count_ = nullptr;
    QTableWidget *table_ = nullptr;
};

} // namespace aerosim
