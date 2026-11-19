#include "aerosim/ui/RadioChoice.h"
#include <QButtonGroup>
#include <QGridLayout>
#include <QRadioButton>
#include <QResizeEvent>

namespace aerosim {
RadioChoice::RadioChoice(QWidget *parent, int columns)
    : QWidget(parent), group_(new QButtonGroup(this)), layout_(new QGridLayout(this)),
      maxColumns_(qMax(1, columns))
{
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setHorizontalSpacing(12);
    layout_->setVerticalSpacing(4);
    layout_->setAlignment(Qt::AlignTop);
    auto policy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
}
void RadioChoice::addItem(const QString &label, const QVariant &data)
{
    const int index = buttons_.size();
    auto *button = new QRadioButton(label, this);
    button->setObjectName(objectName() + QLatin1Char('_') + data.toString());
    button->ensurePolished();
    button->setMinimumWidth(button->sizeHint().width() + 2);
    group_->addButton(button, index);
    buttons_.append(button);
    data_.append(data);
    setMinimumWidth(qMax(minimumWidth(), button->minimumWidth()));
    connect(button, &QRadioButton::toggled, this, [this, index](bool checked) {
        if (checked) emit currentIndexChanged(index);
    });
    if (index == 0) button->setChecked(true);
    columns_ = 0;
    arrange(columnsForWidth(width()));
}
int RadioChoice::findData(const QVariant &data) const { return data_.indexOf(data); }
int RadioChoice::currentIndex() const { return group_->checkedId(); }
QVariant RadioChoice::currentData() const { return data_.value(currentIndex()); }
QString RadioChoice::currentText() const
{
    return group_->checkedButton() ? group_->checkedButton()->text() : QString();
}
void RadioChoice::setCurrentIndex(int index)
{
    if (index >= 0 && index < buttons_.size()) buttons_[index]->setChecked(true);
}
int RadioChoice::widthForColumns(int columns) const
{
    int result = 12 * (columns - 1);
    for (int column = 0; column < columns; ++column) {
        int itemWidth = 0;
        for (int index = column; index < buttons_.size(); index += columns)
            itemWidth = qMax(itemWidth, buttons_[index]->minimumWidth());
        result += itemWidth;
    }
    return result;
}
int RadioChoice::columnsForWidth(int width) const
{
    for (int columns = qMin(maxColumns_, buttons_.size()); columns > 1; --columns)
        if (widthForColumns(columns) <= width) return columns;
    return 1;
}
int RadioChoice::heightForWidth(int width) const
{
    const int columns = columnsForWidth(width);
    const int rows = (buttons_.size() + columns - 1) / columns;
    int height = 0;
    for (auto *button : buttons_) height = qMax(height, button->sizeHint().height());
    return rows * height + qMax(0, rows - 1) * 4;
}
QSize RadioChoice::minimumSizeHint() const
{
    QSize result;
    for (auto *button : buttons_) result = result.expandedTo(QSize(button->minimumWidth(), button->sizeHint().height()));
    return result;
}
QSize RadioChoice::sizeHint() const
{
    const int preferredWidth = widthForColumns(qMax(1, qMin(maxColumns_, buttons_.size())));
    return QSize(preferredWidth, heightForWidth(width()));
}
void RadioChoice::arrange(int columns)
{
    if (columns_ == columns) return;
    for (int i = 0; i < maxColumns_; ++i) layout_->setColumnStretch(i, 0);
    while (auto *item = layout_->takeAt(0)) delete item;
    for (int i = 0; i < buttons_.size(); ++i)
        layout_->addWidget(buttons_[i], i / columns, i % columns);
    for (int i = 0; i < columns; ++i) layout_->setColumnStretch(i, 1);
    columns_ = columns;
    const int height = heightForWidth(width());
    setFixedHeight(height);
    layout_->activate();
    updateGeometry();
    emit layoutHeightChanged(height);
}
void RadioChoice::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    arrange(columnsForWidth(event->size().width()));
}
}
