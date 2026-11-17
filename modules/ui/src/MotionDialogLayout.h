#pragma once
#include "aerosim/ui/UiLayoutConfig.h"
#include <QBoxLayout>
#include <QDialog>
#include <QResizeEvent>

namespace aerosim {
namespace motionLayout {
// Reflow existing widgets without rebuilding editors or losing their drafts.
class Columns final : public QWidget {
public:
    explicit Columns(QWidget *parent) : QWidget(parent), items(new QBoxLayout(QBoxLayout::LeftToRight, this)) {
        items->setContentsMargins(0, 0, 0, 0);
        items->setSpacing(16);
        items->setAlignment(Qt::AlignTop);
    }
    QBoxLayout *items;
protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        items->setDirection(event->size().width() >= 960 ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    }
};
inline void fit(QDialog *dialog, const QSize &preferred) {
    QScreen *screen = uimetrics::screenOf(dialog);
    if (!screen) { dialog->resize(preferred); return; }
    const QRect available = uimetrics::availableRect(dialog);
    const QSize limit(qMax(1, available.width() - 48), qMax(1, available.height() - 80));
    dialog->resize(preferred.boundedTo(limit));
    dialog->move(available.center() - QPoint(dialog->width() / 2, dialog->height() / 2));
}
}
}
