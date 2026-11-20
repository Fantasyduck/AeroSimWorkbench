#pragma once

#include <QWidget>
#include <QVariant>
#include <QVector>

class QButtonGroup;
class QGridLayout;
class QRadioButton;

namespace aerosim {

// A small, explicit choice set. Stable item data is shared with draft models.
class RadioChoice final : public QWidget {
    Q_OBJECT
public:
    explicit RadioChoice(QWidget *parent = nullptr, int columns = 2);
    void addItem(const QString &label, const QVariant &data);
    int findData(const QVariant &data) const;
    int currentIndex() const;
    QVariant currentData() const;
    QString currentText() const;
    void setCurrentIndex(int index);
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
signals:
    void currentIndexChanged(int index);
    void layoutHeightChanged(int height);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    int widthForColumns(int columns) const;
    int columnsForWidth(int width) const;
    void arrange(int columns);
    QButtonGroup *group_;
    QGridLayout *layout_;
    QVector<QRadioButton *> buttons_;
    QVector<QVariant> data_;
    int maxColumns_;
    int columns_ = 0;
};
}
