#pragma once

#include "aerosim/ui/IntegrationPorts.h"

#include <QMainWindow>

class QLabel;
class QTextEdit;
class QPushButton;
class QVBoxLayout;

namespace aerosim {

// PLACEHOLDER for B. No SARibbon, VTK viewer, or business UI is claimed.
class MockWindow final : public QMainWindow, public IWidgetHost {
    Q_OBJECT
public:
    explicit MockWindow(QWidget *parent = nullptr);
    QWidget *windowWidget() override { return this; }
    bool insertWidget(WidgetSlot slot, QWidget *widget) override;
    void bindServices(const ServiceBindings &services) override;
    void appendMessage(const QString &message);

signals:
    void postprocessDemoRequested();

private:
    QVBoxLayout *viewportLayout_ = nullptr;
    QTextEdit *events_ = nullptr;
    QPushButton *sendButton_ = nullptr;
    QPushButton *postprocessButton_ = nullptr;
    ServiceBindings services_;
    bool bound_ = false;
};

} // namespace aerosim
