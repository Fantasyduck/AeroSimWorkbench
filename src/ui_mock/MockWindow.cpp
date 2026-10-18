#include "MockWindow.h"

#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace aerosim {

MockWindow::MockWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("AeroSimWorkbench - MOCK GUI shell"));
    resize(960, 620);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->addWidget(new QLabel(QStringLiteral("A-owned startup shell; B's MainWindow will replace this"), central));
    sendButton_ = new QPushButton(QStringLiteral("Send MOCK TaskCommand"), central);
    sendButton_->setEnabled(false);
    layout->addWidget(sendButton_);
    auto *viewport = new QWidget(central);
    viewportLayout_ = new QVBoxLayout(viewport);
    layout->addWidget(viewport, 1);
    events_ = new QTextEdit(central);
    events_->setReadOnly(true);
    events_->setMaximumHeight(150);
    layout->addWidget(events_);
    setCentralWidget(central);
}

bool MockWindow::insertWidget(WidgetSlot slot, QWidget *widget)
{
    if (slot != WidgetSlot::Viewport || !widget || !viewportLayout_ || viewportLayout_->count() != 0)
        return false;
    viewportLayout_->addWidget(widget); // Qt reparents and owns it.
    return true;
}

void MockWindow::bindServices(const ServiceBindings &services)
{
    if (bound_)
        return;
    services_ = services;
    bound_ = true;
    sendButton_->setEnabled(services_.workflow != nullptr);
    if (!services_.workflow || !services_.tasks)
        return;
    connect(sendButton_, &QPushButton::clicked, this, [this] {
        RunRequest request;
        request.scope.projectId = QStringLiteral("mock-project");
        request.scope.caseId = QStringLiteral("mock-case");
        request.inputSnapshot.snapshotId = QStringLiteral("mock-snapshot");
        services_.workflow->requestRun(request);
        events_->append(QStringLiteral("MOCK command queued"));
    });
    connect(services_.tasks, &ITaskService::taskEventOccurred, this,
            [this](const TaskEvent &event) { events_->append(event.message); },
            Qt::QueuedConnection);
}

} // namespace aerosim
