#include "aerosim/ui/UiLayoutConfig.h"

#include <QGuiApplication>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

namespace aerosim {
namespace uimetrics {

QScreen *screenOf(const QWidget *widget)
{
    if (widget) {
        if (auto *window = widget->window()) {
            if (window->windowHandle()) {
                if (auto *screen = window->windowHandle()->screen()) return screen;
            }
        }
    }
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty()) return nullptr;
    if (widget && widget->window() && widget->window()->windowHandle()) {
        const QPoint center = widget->window()->frameGeometry().center();
        for (auto *screen : screens) {
            if (screen->availableGeometry().contains(center)) return screen;
        }
    }
    return QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen() : screens.first();
}

QRect availableRect(const QWidget *widget)
{
    if (auto *screen = screenOf(widget)) return screen->availableGeometry();
    return QRect(0, 0, detail::kDesignWidth, detail::kDesignHeight);
}

qreal logicalDpi(const QWidget *widget)
{
    if (auto *screen = screenOf(widget)) {
        const qreal dpi = screen->logicalDotsPerInch();
        if (dpi > 0.0) return dpi;
    }
    return 96.0;
}

qreal scaleFactor(const QWidget *widget)
{
    const qreal dpi = logicalDpi(widget);
    return dpi > 0.0 ? dpi / 96.0 : 1.0;
}

QVBoxLayout *createPageRootLayout(QWidget *parent)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(pageMargins());
    layout->setSpacing(panelSpacing());
    layout->setSizeConstraint(QLayout::SetDefaultConstraint);
    return layout;
}

QVBoxLayout *createWorkbenchRootLayout(QWidget *parent)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->setSizeConstraint(QLayout::SetDefaultConstraint);
    return layout;
}

} // namespace uimetrics
} // namespace aerosim
