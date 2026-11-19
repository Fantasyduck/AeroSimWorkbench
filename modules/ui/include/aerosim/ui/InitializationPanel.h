#pragma once
#include <QVariantMap>
#include <QWidget>
class QComboBox;
class QFormLayout;
class QLabel;
class QTableWidget;
namespace aerosim {
class RadioChoice;
class InitializationPanel final : public QWidget {
    Q_OBJECT
public:
    explicit InitializationPanel(const QVariantMap &draft, QWidget *parent = nullptr);
    void setSources(const QVariantMap &boundary, const QVariantMap &reference);
signals:
    void draftChanged(const QVariantMap &draft);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void updateGlobal();
    void rebuildRegions(int count);
    void publish();
    QVariantMap draft_, reference_;
    QVariantList boundaries_, regions_;
    RadioChoice *mode_ = nullptr;
    QFormLayout *globalForm_ = nullptr;
    QComboBox *source_ = nullptr;
    QLabel *sourceLabel_ = nullptr;
    QLabel *sourceHint_ = nullptr;
    QWidget *state_ = nullptr;
    QTableWidget *table_ = nullptr;
};
}
