#include "aerosim/ui/MainWindow.h"

#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QClipboard>
#include <QMenu>
#include <QSettings>
#include <QPalette>
#include <QComboBox>
#include <QDateTime>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeySequence>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QTimer>
#include <QResource>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <cstddef>

// Keep only this module's generated resource object in the static UI library.
// SARibbon's own resources are initialized by SARibbon itself.
static QWidget *initializeRibbonResources(QWidget *parent)
{
    Q_INIT_RESOURCE(RibbonIcons);
    return parent;
}

namespace aerosim {
namespace {
enum NavigationRole {
    PageRole = Qt::UserRole,
    ObjectIdRole,
    NodeTypeRole,
    TaskStatusRole
};

// Match the postprocess UI: explicit UTF-16 literals avoid source/execution
// code-page dependence in the Qt 5.12 / MSVC 2017 toolchain.
template <std::size_t N>
QString uiText(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}

QString taskStatus(TaskStatus status)
{
    switch (status) {
    case TaskStatus::Preparing: return uiText(u"\u51c6\u5907\u4e2d");
    case TaskStatus::Running: return uiText(u"\u8fd0\u884c\u4e2d");
    case TaskStatus::Paused: return uiText(u"\u5df2\u6682\u505c");
    case TaskStatus::Finished: return uiText(u"\u5df2\u5b8c\u6210");
    case TaskStatus::Failed: return uiText(u"\u5931\u8d25");
    case TaskStatus::Cancelled: return uiText(u"\u5df2\u53d6\u6d88");
    default: return uiText(u"\u672a\u77e5");
    }
}
QString stepStatus(StepState state)
{
    switch (state) {
    case StepState::Pending: return uiText(u"\u5f85\u914d\u7f6e");
    case StepState::NeedsValidation: return uiText(u"\u5f85\u6821\u9a8c");
    case StepState::Complete: return uiText(u"\u5df2\u5b8c\u6210");
    case StepState::Blocked: return uiText(u"\u88ab\u963b\u585e");
    }
    return {};
}
QLabel *emptyState(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setMargin(20);
    return label;
}

QTreeWidgetItem *addNavigationItem(QTreeWidgetItem *parent, const QString &text,
                                   const QString &pageId, const QString &objectId,
                                   const QString &nodeType)
{
    auto *item = new QTreeWidgetItem(parent, {text});
    item->setData(0, PageRole, pageId);
    item->setData(0, ObjectIdRole, objectId);
    item->setData(0, NodeTypeRole, nodeType);
    return item;
}

QIcon statusIcon(TaskStatus status)
{
    QColor color(QStringLiteral("#8a94a3"));
    switch (status) {
    case TaskStatus::Preparing: color = QColor(QStringLiteral("#d9a300")); break;
    case TaskStatus::Running: color = QColor(QStringLiteral("#2b78d0")); break;
    case TaskStatus::Paused: color = QColor(QStringLiteral("#8a5db7")); break;
    case TaskStatus::Finished: color = QColor(QStringLiteral("#2e9b55")); break;
    case TaskStatus::Failed: color = QColor(QStringLiteral("#c83c3c")); break;
    case TaskStatus::Cancelled: color = QColor(QStringLiteral("#68717d")); break;
    default: break;
    }
    QPixmap marker(12, 12);
    marker.fill(color);
    return QIcon(marker);
}

void setTaskItemStatus(QTreeWidgetItem *item, TaskStatus status)
{
    item->setData(0, TaskStatusRole, static_cast<int>(status));
    item->setIcon(0, statusIcon(status));
    item->setToolTip(0, uiText(u"\u72b6\u6001\uff1a%1\uff08Mock \u754c\u9762\u793a\u4f8b\uff09").arg(taskStatus(status)));
}
}

MainWindow::MainWindow(QWidget *parent) : SARibbonMainWindow(initializeRibbonResources(parent))
{
    setObjectName(QStringLiteral("AeroSimMainWindow"));
    setWindowTitle(uiText(u"AeroSimWorkbench \u00b7 \u4eff\u771f\u5de5\u4f5c\u53f0\uff08Mock \u670d\u52a1\uff09"));
    resize(1440, 960);
    setMinimumSize(1000, 720);
    setDockNestingEnabled(true);
    createWorkspace();
    createRibbon();
    status_ = new QLabel(uiText(u"\u5c31\u7eea  |  \u5de5\u7a0b\uff1a\u672a\u6253\u5f00  |  \u7f51\u683c\uff1a\u672a\u8f7d\u5165  |  \u6c42\u89e3\u5668\uff1a\u672a\u63a5\u5165  |  MPI\uff1a\u2014"), this);
    statusBar()->addWidget(status_, 1);
    statusBar()->addPermanentWidget(new QLabel(uiText(u"UI \u6846\u67b6 \u00b7 Mock \u670d\u52a1"), this));
    QTimer::singleShot(0, this, [this] {
        QSettings settings(QStringLiteral("AeroSimWorkbench"), QStringLiteral("Appearance"));
        applyWindowTheme(settings.value(QStringLiteral("theme"), QStringLiteral("office-blue")).toString());
    });
    restoreLayout();
    appendMessage(uiText(u"\u4e3b\u754c\u9762\u5df2\u5c31\u7eea\u3002\u5de5\u7a0b\u6811\u4e0e\u53c2\u6570\u4e3a\u754c\u9762\u793a\u4f8b\uff1b\u5c1a\u672a\u63a5\u5165\u5de5\u7a0b\u3001\u914d\u7f6e\u3001VTK \u548c\u771f\u5b9e\u6c42\u89e3\u670d\u52a1\u3002"));
}

void MainWindow::createRibbon()
{
    ribbonBar()->setRibbonStyle(SARibbonBar::RibbonStyleLooseThreeRow);
    struct Item { const char *id; QString label; bool large; const char *route; };
    auto addGroup = [this](SARibbonCategory *page, const char *id, const QString &title,
                          std::initializer_list<Item> items) {
        auto *panel = page->addPanel(title);
        panel->setObjectName(QString::fromLatin1(id));
        for (const auto &item : items) {
            const QString key = QString::fromLatin1(item.id);
            const QString path = QStringLiteral(":/aerosim/ribbon/") + key + QStringLiteral(".svg");
            QIcon icon(path);
            auto *action = new QAction(icon, item.label, this);
            action->setObjectName(key + QStringLiteral("Action"));
            action->setProperty("ribbonGroup", QString::fromLatin1(id));
            if (item.large) panel->addLargeAction(action);
            else panel->addSmallAction(action);
            const QString route = QString::fromLatin1(item.route);
            action->setEnabled(!route.isEmpty());
            action->setToolTip(item.label + (route.isEmpty()
                ? uiText(u"\uff1a\u7b49\u5f85\u4e1a\u52a1\u6a21\u5757\u63a5\u5165") : uiText(u"\uff1a\u6253\u5f00\u5bf9\u5e94\u9762\u677f")));
            if (key.startsWith(QStringLiteral("view-")))
                action->setToolTip(uiText(u"\u7b49\u5f85 VTK \u89c6\u53e3\u53ca\u89c6\u56fe\u63a7\u5236\u63a5\u53e3\u63a5\u5165"));
            if (route.startsWith("page:")) {
                const QString pageId = route.mid(5);
                action->setToolTip(uiText(u"\u6253\u5f00\u53c2\u6570\u8349\u7a3f\uff1b\u5c1a\u672a\u6301\u4e45\u5316"));
                connect(action, &QAction::triggered, this, [this, pageId] { selectPage(pageId); });
            } else if (route.startsWith("output:")) {
                const int index = route.mid(7).toInt();
                connect(action, &QAction::triggered, this, [this, index] { showOutput(index); });
            } else if (route.startsWith("workspace:")) {
                const QString workspace = route.mid(10);
                connect(action, &QAction::triggered, this, [this, workspace] { showWorkspacePage(workspace); });
            } else if (route == "postprocess") {
                action->setObjectName(QStringLiteral("openPostProcessAction"));
                connect(action, &QAction::triggered, this, &MainWindow::openPostProcessRequested);
            } else if (route == "run") {
                runAction_ = action;
                action->setObjectName(QStringLiteral("runAction"));
                action->setEnabled(false);
                action->setToolTip(uiText(u"\u4e00\u6b21\u8ba1\u7b97\u5bf9\u5e94\u4e00\u4e2a\u72b6\u6001\uff1b\u7b49\u5f85\u5de5\u4f5c\u6d41\u63d0\u4f9b\u6709\u6548\u8f93\u5165\u4e0e\u8fd0\u884c\u6743\u9650"));
                connect(action, &QAction::triggered, this, &MainWindow::submitRun);
            } else if (route == "layout") {
                connect(action, &QAction::triggered, this, &MainWindow::restoreLayout);
            } else if (route.startsWith("dock:")) {
                auto *dock = route == "dock:project" ? projectDock_
                    : route == "dock:property" ? propertyDock_ : outputDock_;
                connect(action, &QAction::triggered, dock, &QWidget::show);
            } else if (route == "help") {
                connect(action, &QAction::triggered, this, [this] {
                    appendMessage(uiText(u"\u5f00\u59cb\uff1a\u5de5\u7a0b\u3001\u6279\u91cf\u7ba1\u7406\u3001\u524d\u5904\u7406\u3001\u4e1a\u52a1\u53c2\u6570\u3002\u4e00\u6b21\u8ba1\u7b97\u5bf9\u5e94\u4e00\u4e2a\u72b6\u6001\uff0c\u72b6\u6001\u5f52\u5c5e\u5de5\u7a0b\uff1b\u6279\u91cf\u7ba1\u7406\u7ec4\u7ec7\u591a\u4e2a\u8ba1\u7b97\u72b6\u6001\u3002\u7070\u8272\u64cd\u4f5c\u7b49\u5f85\u670d\u52a1\u63a5\u5165\uff0c\u53c2\u6570\u8349\u7a3f\u4e0d\u4ee3\u8868\u5df2\u4fdd\u5b58\u7684\u8ba1\u7b97\u914d\u7f6e\u3002"));
                    showOutput(0);
                });
            }
        }
        return panel;
    };
    auto *start = ribbonBar()->addCategoryPage(uiText(u"\u5f00\u59cb"));
    start->setObjectName(QStringLiteral("startCategory"));
    auto *projectPanel = addGroup(start, "projectGroup", uiText(u"\u5de5\u7a0b"), {
        {"open-project", uiText(u"\u6253\u5f00\u5de5\u7a0b"), false, ""},
        {"save-project", uiText(u"\u4fdd\u5b58\u5de5\u7a0b"), false, ""},
        {"state", uiText(u"\u8ba1\u7b97\u72b6\u6001"), false, "page:states"}});
    auto *newProjectMenu = new QMenu(uiText(u"\u65b0\u5efa\u5de5\u7a0b"), this);
    newProjectMenu->setObjectName(QStringLiteral("newProjectMenu"));
    newProjectMenu->setIcon(QIcon(QStringLiteral(":/aerosim/ribbon/new-project.svg")));
    const struct { const char *id; QString label; const char *icon; } projectEntries[] = {
        {"blankProjectMenuAction", uiText(u"\u7a7a\u767d\u5de5\u7a0b"), "new-project"},
        {"templateProjectMenuAction", uiText(u"\u4ece\u6a21\u677f\u65b0\u5efa"), "template"},
        {"importProjectMenuAction", uiText(u"\u5bfc\u5165\u5de5\u7a0b\u914d\u7f6e"), "import-config"}};
    for (const auto &entry : projectEntries) {
        auto *action = newProjectMenu->addAction(
            QIcon(QStringLiteral(":/aerosim/ribbon/") + QString::fromLatin1(entry.icon) + QStringLiteral(".svg")),
            entry.label);
        action->setObjectName(QString::fromLatin1(entry.id));
        action->setEnabled(false);
        action->setToolTip(uiText(u"\u7b49\u5f85\u5de5\u7a0b\u670d\u52a1\u63a5\u5165"));
    }
    newProjectMenu->menuAction()->setObjectName(QStringLiteral("newProjectMenuAction"));
    newProjectMenu->menuAction()->setProperty("ribbonGroup", QStringLiteral("projectGroup"));
    newProjectMenu->menuAction()->setToolTip(uiText(u"\u9009\u62e9\u65b0\u5efa\u5de5\u7a0b\u7684\u65b9\u5f0f"));
    projectPanel->addLargeMenu(newProjectMenu);
    projectPanel->moveAction(projectPanel->actionIndex(newProjectMenu->menuAction()), 0);

    auto *batchPanel = addGroup(start, "batchGroup", uiText(u"\u6279\u91cf\u7ba1\u7406"), {
        {"new-state", uiText(u"\u65b0\u5efa\u72b6\u6001"), false, ""},
        {"template", uiText(u"\u72b6\u6001\u6a21\u677f"), false, ""},
        {"queue", uiText(u"\u4efb\u52a1\u5217\u8868"), false, "workspace:batch-monitor"}});
    auto *batchMenu = new QMenu(uiText(u"\u6279\u91cf\u914d\u7f6e"), this);
    batchMenu->setObjectName(QStringLiteral("batchMenu"));
    batchMenu->setIcon(QIcon(QStringLiteral(":/aerosim/ribbon/batch.svg")));
    auto *openBatch = batchMenu->addAction(
        QIcon(QStringLiteral(":/aerosim/ribbon/state.svg")), uiText(u"\u6253\u5f00\u72b6\u6001\u914d\u7f6e"));
    openBatch->setObjectName(QStringLiteral("openBatchConfigurationAction"));
    openBatch->setToolTip(uiText(u"\u6253\u5f00\u8ba1\u7b97\u72b6\u6001\u53c2\u6570\u8349\u7a3f"));
    connect(openBatch, &QAction::triggered, this, [this, start] {
        ribbonBar()->raiseCategory(start);
        navigationStack_->setCurrentWidget(projectTree_);
        selectNavigationNode(projectTree_, QStringLiteral("batch-plans"));
    });
    auto *importStates = batchMenu->addAction(
        QIcon(QStringLiteral(":/aerosim/ribbon/import-config.svg")), uiText(u"\u5bfc\u5165\u72b6\u6001\u8868"));
    importStates->setObjectName(QStringLiteral("importStateTableAction"));
    importStates->setEnabled(false);
    auto *batchTemplate = batchMenu->addAction(
        QIcon(QStringLiteral(":/aerosim/ribbon/template.svg")), uiText(u"\u6279\u91cf\u53c2\u6570\u6a21\u677f"));
    batchTemplate->setObjectName(QStringLiteral("batchTemplateMenuAction"));
    batchTemplate->setEnabled(false);
    batchMenu->menuAction()->setObjectName(QStringLiteral("batchMenuAction"));
    batchMenu->menuAction()->setProperty("ribbonGroup", QStringLiteral("batchGroup"));
    batchMenu->menuAction()->setToolTip(uiText(u"\u6279\u91cf\u7ec4\u7ec7\u591a\u4e2a\u8ba1\u7b97\u72b6\u6001"));
    batchPanel->addLargeMenu(batchMenu);
    batchPanel->moveAction(batchPanel->actionIndex(batchMenu->menuAction()), 0);
    addGroup(start, "preprocessGroup", uiText(u"\u524d\u5904\u7406"), {
        {"mesh", uiText(u"\u5bfc\u5165\u7f51\u683c"), true, ""},
        {"transform", uiText(u"\u7f51\u683c\u53d8\u6362"), false, "page:mesh"},
        {"boundary", uiText(u"\u8fb9\u754c\u6761\u4ef6"), false, "page:boundary"},
        {"initialize", uiText(u"\u521d\u59cb\u5316"), false, ""}});
    addGroup(start, "parametersGroup", uiText(u"\u4e1a\u52a1\u53c2\u6570"), {
        {"physics", uiText(u"\u7269\u7406\u6a21\u578b"), true, "page:physics"},
        {"atmosphere", uiText(u"\u5927\u6c14\u53c2\u6570"), false, "page:physics"},
        {"settings", uiText(u"\u8ba1\u7b97\u53c2\u6570"), false, "page:solver"},
        {"parallel", uiText(u"\u5e76\u884c\u8bbe\u7f6e"), false, "page:parallel"},
        {"import-config", uiText(u"\u5bfc\u5165\u914d\u7f6e"), false, ""},
        {"export-config", uiText(u"\u5bfc\u51fa\u914d\u7f6e"), false, ""}});
    auto *viewPanel = addGroup(start, "viewGroup", uiText(u"\u89c6\u56fe"), {
        {"view-fit", uiText(u"\u9002\u5e94"), true, ""},
        {"view-zoom-in", uiText(u"\u653e\u5927"), false, ""},
        {"view-zoom-out", uiText(u"\u7f29\u5c0f"), false, ""},
        {"view-rotate", uiText(u"\u65cb\u8f6c"), false, ""},
        {"view-front", uiText(u"\u6b63\u89c6 X"), false, ""},
        {"view-side", uiText(u"\u4fa7\u89c6 Y"), false, ""},
        {"view-top", uiText(u"\u9876\u89c6 Z"), false, ""}});
    auto *displayMenu = new QMenu(uiText(u"\u663e\u793a\u65b9\u5f0f"), this);
    displayMenu->setObjectName(QStringLiteral("displayModeMenu"));
    displayMenu->setIcon(QIcon(QStringLiteral(":/aerosim/ribbon/view-surface.svg")));
    auto *surface = displayMenu->addAction(
        QIcon(QStringLiteral(":/aerosim/ribbon/view-surface.svg")), uiText(u"\u8868\u9762"));
    surface->setObjectName(QStringLiteral("surfaceModeMenuAction"));
    surface->setEnabled(false);
    auto *wireframe = displayMenu->addAction(
        QIcon(QStringLiteral(":/aerosim/ribbon/view-wireframe.svg")), uiText(u"\u7ebf\u6846"));
    wireframe->setObjectName(QStringLiteral("wireframeModeMenuAction"));
    wireframe->setEnabled(false);
    displayMenu->menuAction()->setObjectName(QStringLiteral("displayModeMenuAction"));
    displayMenu->menuAction()->setProperty("ribbonGroup", QStringLiteral("viewGroup"));
    displayMenu->menuAction()->setToolTip(uiText(u"\u7b49\u5f85 VTK \u663e\u793a\u6a21\u5f0f\u63a5\u53e3\u63a5\u5165"));
    viewPanel->addSmallMenu(displayMenu);
    auto *monitor = ribbonBar()->addCategoryPage(uiText(u"\u76d1\u63a7"));
    monitor->setObjectName(QStringLiteral("monitorCategory"));
    addGroup(monitor, "calculationGroup", uiText(u"\u5f53\u524d\u8ba1\u7b97\u72b6\u6001"), {
        {"run", uiText(u"\u5f00\u59cb\u8ba1\u7b97"), true, "run"},
        {"pause", uiText(u"\u6682\u505c"), false, ""},
        {"resume", uiText(u"\u7ee7\u7eed\u8ba1\u7b97"), false, ""},
        {"stop", uiText(u"\u505c\u6b62"), false, ""}});
    addGroup(monitor, "monitorGroup", uiText(u"\u8fd0\u884c\u76d1\u63a7"), {
        {"monitor", uiText(u"\u8ba1\u7b97\u76d1\u63a7"), true, "workspace:task-monitor"},
        {"log", uiText(u"\u65e5\u5fd7"), false, "output:0"},
        {"residual", uiText(u"\u6536\u655b\u66f2\u7ebf"), false, "workspace:task-monitor"},
        {"errors", uiText(u"\u9519\u8bef\u4fe1\u606f"), false, "output:3"}});
    auto *post = ribbonBar()->addCategoryPage(uiText(u"\u540e\u5904\u7406"));
    post->setObjectName(QStringLiteral("postprocessCategory"));
    addGroup(post, "workspaceGroup", uiText(u"\u5de5\u4f5c\u533a"), {
        {"workbench", uiText(u"\u540e\u5904\u7406\u5de5\u4f5c\u53f0"), true, "postprocess"}});
    addGroup(post, "fieldGroup", uiText(u"\u573a\u663e\u793a"), {
        {"contour", uiText(u"\u4e91\u56fe"), true, ""},
        {"slice", uiText(u"\u5207\u9762"), false, ""},
        {"iso", uiText(u"\u7b49\u503c\u9762"), false, ""},
        {"stream", uiText(u"\u6d41\u7ebf"), false, ""}});
    addGroup(post, "resultsGroup", uiText(u"\u7ed3\u679c\u5c55\u793a"), {
        {"curve", uiText(u"\u66f2\u7ebf"), true, ""},
        {"animation", uiText(u"\u52a8\u753b"), true, ""}});
    auto *data = ribbonBar()->addCategoryPage(uiText(u"\u6570\u636e\u5904\u7406"));
    data->setObjectName(QStringLiteral("dataCategory"));
    addGroup(data, "stateResultsGroup", uiText(u"\u72b6\u6001\u6570\u636e"), {
        {"compare", uiText(u"\u591a\u72b6\u6001\u6570\u636e\u5904\u7406"), true, ""}});
    auto *help = ribbonBar()->addCategoryPage(uiText(u"\u5e2e\u52a9"));
    help->setObjectName(QStringLiteral("helpCategory"));
    addGroup(help, "guideGroup", uiText(u"\u4f7f\u7528\u5e2e\u52a9"), {
        {"help", uiText(u"\u4f7f\u7528\u8bf4\u660e"), true, "help"},
        {"about", uiText(u"\u5173\u4e8e"), false, "help"}});
    addGroup(help, "toolsGroup", uiText(u"\u5de5\u5177"), {
        {"layout", uiText(u"\u6062\u590d\u9ed8\u8ba4\u5e03\u5c40"), true, "layout"},
        {"navigation", uiText(u"\u5de5\u7a0b\u5bfc\u822a"), false, "dock:project"},
        {"property", uiText(u"\u5c5e\u6027\u9762\u677f"), false, "dock:property"},
        {"output", uiText(u"\u8f93\u51fa\u9762\u677f"), false, "dock:output"}});
    auto *appearance = help->addPanel(uiText(u"\u5916\u89c2"));
    appearance->setObjectName(QStringLiteral("appearanceGroup"));
    auto *menu = new QMenu(uiText(u"\u5207\u6362\u76ae\u80a4"), this);
    menu->setObjectName(QStringLiteral("themeMenu"));
    menu->setIcon(QIcon(QStringLiteral(":/aerosim/ribbon/skin.svg")));
    auto *group = new QActionGroup(this);
    group->setExclusive(true);
    struct ThemeItem { const char *id; QString label; };
    const ThemeItem themes[] = {{"office-blue", uiText(u"Office \u84dd\u8272")}, {"office-light", uiText(u"Office \u6d45\u8272")}, {"dark", uiText(u"\u6df1\u8272")}, {"dark-gray", uiText(u"\u6df1\u7070\u8272")}};
    for (const auto &theme : themes) {
        const QString id = QString::fromLatin1(theme.id);
        auto *action = menu->addAction(theme.label);
        action->setObjectName(QStringLiteral("theme-") + id);
        action->setData(id);
        action->setCheckable(true);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, id] {
            applyWindowTheme(id);
            QSettings settings(QStringLiteral("AeroSimWorkbench"), QStringLiteral("Appearance"));
            settings.setValue(QStringLiteral("theme"), id);
        });
    }
    appearance->addLargeMenu(menu);
    connect(ribbonBar(), &SARibbonBar::currentRibbonTabChanged,
            this, &MainWindow::switchNavigationForCategory);
}

void MainWindow::applyWindowTheme(const QString &requestedId)
{
    QString id = requestedId;
    SARibbonTheme theme = SARibbonTheme::RibbonThemeOffice2016Blue;
    if (id == "office-light") theme = SARibbonTheme::RibbonThemeOffice2013;
    else if (id == "dark") theme = SARibbonTheme::RibbonThemeDark;
    else if (id == "dark-gray") theme = SARibbonTheme::RibbonThemeDark2;
    else id = QStringLiteral("office-blue");
    if (property("windowThemeId").toString() == id) return;
    const bool dark = id == "dark" || id == "dark-gray";
    const bool officeBlue = id == "office-blue";
    const QString bg = dark ? (id == "dark" ? "#20242b" : "#353535") : "#f4f7fb";
    const QString base = dark ? "#292e36" : "#ffffff";
    const QString text = dark ? "#e6eaf0" : "#25364c";
    const QString border = dark ? "#59616d" : "#dce2eb";
    const QString selected = dark ? "#355a80" : "#dceafd";
    QPalette colors = palette();
    colors.setColor(QPalette::Window, QColor(bg));
    colors.setColor(QPalette::Base, QColor(base));
    colors.setColor(QPalette::AlternateBase, QColor(bg));
    colors.setColor(QPalette::WindowText, QColor(text));
    colors.setColor(QPalette::Text, QColor(text));
    colors.setColor(QPalette::Button, QColor(bg));
    colors.setColor(QPalette::ButtonText, QColor(text));
    colors.setColor(QPalette::Highlight, QColor(selected));
    colors.setColor(QPalette::HighlightedText, QColor(text));
    colors.setColor(QPalette::Disabled, QPalette::Text, QColor(dark ? "#9ca5b0" : "#808080"));
    colors.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(dark ? "#9ca5b0" : "#808080"));
    setPalette(colors);
    // SARibbon replaces its QSS on every call. Append workspace styling once,
    // avoiding accumulated overrides when the user switches themes repeatedly.
    setRibbonTheme(theme);
    const QColor wordBlue(QStringLiteral("#2b579a"));
    ribbonBar()->setWindowTitleBackgroundBrush(
        officeBlue ? QBrush(wordBlue) : QBrush(Qt::NoBrush));
    ribbonBar()->setWindowTitleTextColor(officeBlue ? QColor(Qt::white) : QColor());
    const QString wordChrome = officeBlue ? QStringLiteral(
        "SARibbonBar { background-color: #2b579a; color: white; border-color: #2b579a; }"
        "SARibbonApplicationWidget { background-color: #2b579a; color: white; }"
        "SARibbonTabBar { background-color: transparent; }"
        "SARibbonTabBar::tab { color: white; background: transparent; border: 1px solid transparent; }"
        "SARibbonTabBar::tab:selected { color: #2b579a; background: #f4f7fb; border: 1px solid #f4f7fb; }"
        "SARibbonTabBar::tab:hover:!selected { color: white; background: #3f6fb2; border: 1px solid #6f92c4; }"
        "SARibbonCategory, SARibbonPanel { background-color: #f4f7fb; color: #25364c; }"
        "SARibbonPanelLabel { background: transparent; color: #5d6b7a; }"
        "SARibbonToolButton { color: #25364c; background: transparent; border: 1px solid transparent; }"
        "SARibbonToolButton:hover { background: #dceafd; border: 1px solid #b6d0ee; }"
        "SARibbonToolButton:pressed, SARibbonToolButton:checked { background: #c8ddf5; border: 1px solid #8fb5df; }"
        "SARibbonPanel > SARibbonButtonGroupWidget { background: #f4f7fb; border-color: #dce2eb; }"
        "SARibbonSystemButtonBar QToolButton { color: white; background: transparent; }"
        "SARibbonSystemButtonBar QToolButton:hover { background: #3f6fb2; }") : QString();
    setStyleSheet(styleSheet() + QStringLiteral(
        "QMainWindow#AeroSimMainWindow, QDockWidget, QDockWidget QWidget, QStatusBar QLabel { background: %1; color: %3; }"
        "QMenu { background: %2; color: %3; border: 1px solid %4; }"
        "QMenu::item:selected { background: %5; }"
        "QDockWidget::title { background: %1; padding: 7px; color: %3; }"
        "QTreeWidget, QTableWidget, QPlainTextEdit, QLineEdit, QComboBox, QAbstractSpinBox { background: %2; color: %3; }"
        "QTreeWidget, QTableWidget { alternate-background-color: %1; }"
        "QTreeWidget { border: 0; }"
        "QTreeWidget::item { padding: 5px 2px; }"
        "QTreeWidget::item:selected { background: %5; color: %3; }"
        "QHeaderView::section { background: %1; color: %3; border: 0; border-right: 1px solid %4; border-bottom: 1px solid %4; padding: 4px 7px; }"
        "QWidget#outputToolbar { background: %1; color: %3; border-bottom: 1px solid %4; }"
        "QLabel#outputTitle { color: %3; }"
        "QTabWidget::pane { border: 1px solid %4; }"
        "QTabWidget QTabBar::tab { background: %1; color: %3; padding: 6px 10px; }"
        "QTabWidget QTabBar::tab:selected { background: %2; }"
        "QStatusBar { background: %1; color: %3; }"
        "QWidget#viewportSlot { background: %1; color: %3; }"
        "QWidget#viewportSlot QLabel { color: %3; }"
        "QLabel#propertyTitle { color: %3; font-size: 16px; font-weight: 600; }"
        "QPushButton { background: %1; color: %3; border: 1px solid %4; padding: 6px 12px; }"
        "QPushButton:hover { background: %5; }").arg(bg, base, text, border, selected)
        + wordChrome);
    const QString editorStyle = QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; border: 1px solid %3; }")
        .arg(base, text, border);
    for (auto *editor : findChildren<QPlainTextEdit *>()) {
        editor->setPalette(colors);
        editor->setStyleSheet(editorStyle);
    }
    setProperty("windowThemeId", id);
    if (auto *menu = findChild<QMenu *>(QStringLiteral("themeMenu")))
        for (auto *action : menu->actions()) action->setChecked(action->data().toString() == id);
}

QWidget *MainWindow::createSlot(WidgetSlot slot, const QString &text)
{
    auto *host = new QWidget(this);
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(emptyState(text, host));
    slotLayouts_.insert(static_cast<int>(slot), layout);
    return host;
}

void MainWindow::createWorkspace()
{
    workspaceStack_ = new QStackedWidget(this);
    workspaceStack_->setObjectName(QStringLiteral("centralWorkspace"));
    viewportPage_ = new QWidget(workspaceStack_);
    viewportPage_->setObjectName(QStringLiteral("viewportPage"));
    auto *viewportLayout = new QVBoxLayout(viewportPage_);
    viewportLayout->setContentsMargins(0, 0, 0, 0);
    auto *viewport = createSlot(WidgetSlot::Viewport, uiText(u"\u4e09\u7ef4\u5de5\u4f5c\u533a\n\nVTK 8.2 \u89c6\u53e3\u63a5\u5165\u4f4d\u7f6e\n\u5bfc\u5165\u4e0e\u6e32\u67d3\u6a21\u5757\u5c1a\u672a\u63a5\u5165"));
    viewport->setObjectName(QStringLiteral("viewportSlot"));
    viewportLayout->addWidget(viewport, 1);
    workspaceStack_->addWidget(viewportPage_);

    const auto createWorkspacePage = [this](const QString &objectName, const QString &title,
                                             const QString &description) {
        auto *page = new QWidget(workspaceStack_);
        page->setObjectName(objectName);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(24, 20, 24, 20);
        auto *heading = new QLabel(title, page);
        heading->setObjectName(QStringLiteral("workspacePageTitle"));
        QFont font = heading->font();
        font.setPointSize(font.pointSize() + 3);
        font.setBold(true);
        heading->setFont(font);
        layout->addWidget(heading);
        auto *note = new QLabel(description, page);
        note->setWordWrap(true);
        layout->addWidget(note);
        return page;
    };
    stateListPage_ = createWorkspacePage(QStringLiteral("stateListPage"), uiText(u"\u5de5\u51b5\u7ba1\u7406"),
        uiText(u"\u5de5\u51b5\u4e0e\u6279\u5904\u7406\u8ba1\u5212\u5206\u5f00\u7ba1\u7406\u3002\u4ee5\u4e0b\u5185\u5bb9\u4ec5\u7528\u4e8e\u754c\u9762\u9a8c\u8bc1\uff0c\u672a\u4ece\u5de5\u7a0b\u670d\u52a1\u52a0\u8f7d\u3002"));
    auto *stateLayout = qobject_cast<QVBoxLayout *>(stateListPage_->layout());
    stateTable_ = new QTableWidget(3, 3, stateListPage_);
    stateTable_->setObjectName(QStringLiteral("stateTable"));
    stateTable_->setHorizontalHeaderLabels({uiText(u"\u5de5\u51b5"), uiText(u"\u53c2\u6570\u6458\u8981"), uiText(u"\u6821\u9a8c\u72b6\u6001")});
    const QStringList stateNames = {QStringLiteral("State_001"), QStringLiteral("State_002"), QStringLiteral("State_003")};
    for (int row = 0; row < stateNames.size(); ++row) {
        stateTable_->setItem(row, 0, new QTableWidgetItem(stateNames[row] + uiText(u"\uff08\u754c\u9762\u793a\u4f8b\uff09")));
        stateTable_->setItem(row, 1, new QTableWidgetItem(uiText(u"\u7b49\u5f85\u5de5\u51b5\u6570\u636e\u6a21\u578b")));
        stateTable_->setItem(row, 2, new QTableWidgetItem(row < 2 ? uiText(u"\u793a\u4f8b\uff1a\u5df2\u901a\u8fc7") : uiText(u"\u793a\u4f8b\uff1a\u5f85\u6821\u9a8c")));
    }
    stateTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    stateTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    stateLayout->addWidget(stateTable_, 1);

    batchPlanPage_ = createWorkspacePage(QStringLiteral("batchPlanPage"), uiText(u"\u6279\u5904\u7406\u8ba1\u5212"),
        uiText(u"BatchPlan \u53ea\u7ec4\u7ec7\u5de5\u51b5\u5f15\u7528\u4e0e\u6267\u884c\u987a\u5e8f\uff0c\u4e0d\u4ee3\u8868\u8ba1\u7b97\u5df2\u542f\u52a8\u3002\u793a\u4f8b\u5217\u4e0d\u4f5c\u4e3a\u6b63\u5f0f\u5de5\u51b5\u5b57\u6bb5\u7ea6\u5b9a\u3002"));
    auto *batchLayout = qobject_cast<QVBoxLayout *>(batchPlanPage_->layout());
    batchPlanTable_ = new QTableWidget(3, 3, batchPlanPage_);
    batchPlanTable_->setObjectName(QStringLiteral("batchPlanTable"));
    batchPlanTable_->setHorizontalHeaderLabels({uiText(u"\u5de5\u51b5"), uiText(u"\u53c2\u6570\u6458\u8981"), uiText(u"\u6821\u9a8c\u72b6\u6001")});
    for (int row = 0; row < stateNames.size(); ++row) {
        batchPlanTable_->setItem(row, 0, new QTableWidgetItem(stateNames[row]));
        batchPlanTable_->setItem(row, 1, new QTableWidgetItem(uiText(u"\u7531\u5de5\u51b5\u6570\u636e\u6a21\u578b\u52a8\u6001\u751f\u6210")));
        batchPlanTable_->setItem(row, 2, new QTableWidgetItem(row < 2 ? uiText(u"\u793a\u4f8b\uff1a\u5df2\u901a\u8fc7") : uiText(u"\u793a\u4f8b\uff1a\u5f85\u6821\u9a8c")));
    }
    batchPlanTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    batchPlanTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    batchPlanTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    batchPlanTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    batchLayout->addWidget(batchPlanTable_, 1);
    auto *batchCommands = new QHBoxLayout;
    const QStringList batchCommandLabels = {uiText(u"\u6dfb\u52a0\u5de5\u51b5"), uiText(u"\u79fb\u9664"), uiText(u"\u4e0a\u79fb"), uiText(u"\u4e0b\u79fb"), uiText(u"\u67e5\u770b\u6821\u9a8c")};
    for (const auto &label : batchCommandLabels) {
        auto *button = new QPushButton(label, batchPlanPage_);
        button->setEnabled(false);
        button->setToolTip(uiText(u"\u7b49\u5f85 BatchPlan \u670d\u52a1\u63a5\u5165"));
        batchCommands->addWidget(button);
    }
    batchCommands->addStretch();
    batchLayout->addLayout(batchCommands);
    auto *newPlan = new QPushButton(uiText(u"\u65b0\u5efa\u6279\u5904\u7406\u8ba1\u5212"), batchPlanPage_);
    newPlan->setEnabled(false);
    newPlan->setToolTip(uiText(u"\u7b49\u5f85\u5de5\u7a0b\u4e0e\u6279\u5904\u7406\u670d\u52a1\u63a5\u5165"));
    batchLayout->addWidget(newPlan, 0, Qt::AlignLeft);

    batchMonitorPage_ = createWorkspacePage(QStringLiteral("batchMonitorPage"), uiText(u"\u6279\u5904\u7406\u603b\u4f53\u76d1\u63a7"),
        uiText(u"Mock \u754c\u9762\u793a\u4f8b\uff1a3 \u4e2a\u5b50\u4efb\u52a1\uff0c\u5df2\u5b8c\u6210 1\uff0c\u8fd0\u884c\u4e2d 1\uff0c\u7b49\u5f85 1\uff0c\u5931\u8d25 0\u3002\u5c1a\u672a\u63a5\u5165\u771f\u5b9e BatchRun \u6570\u636e\u3002"));
    auto *batchMonitorLayout = qobject_cast<QVBoxLayout *>(batchMonitorPage_->layout());
    auto *batchProgress = new QProgressBar(batchMonitorPage_);
    batchProgress->setObjectName(QStringLiteral("batchRunProgress"));
    batchProgress->setRange(0, 3);
    batchProgress->setValue(1);
    batchProgress->setFormat(uiText(u"Mock \u793a\u4f8b\uff1a%v / %m \u5df2\u5b8c\u6210"));
    batchMonitorLayout->addWidget(batchProgress);
    auto *batchSummary = new QLabel(uiText(u"\u603b\u6570 3    \u8fd0\u884c\u4e2d 1    \u5df2\u5b8c\u6210 1    \u5931\u8d25 0    \u7b49\u5f85 1    \uff08Mock\uff09"), batchMonitorPage_);
    batchSummary->setObjectName(QStringLiteral("batchRunSummary"));
    batchMonitorLayout->addWidget(batchSummary);
    batchMonitorLayout->addStretch();
    taskMonitorPage_ = createWorkspacePage(QStringLiteral("taskMonitorPage"), uiText(u"\u5355\u4efb\u52a1\u76d1\u63a7"),
        uiText(u"\u7efc\u5408\u663e\u793a\u4efb\u52a1\u8fdb\u5ea6\u3001\u6536\u655b\u8d8b\u52bf\u548c\u8d44\u6e90\u4fe1\u606f\u3002\u672a\u63a5\u5165\u771f\u5b9e\u6570\u636e\u7684\u5b57\u6bb5\u5747\u6807\u8bb0\u4e3a Mock\u3002"));
    dataProcessPage_ = createWorkspacePage(QStringLiteral("dataProcessPage"), uiText(u"\u6570\u636e\u5904\u7406\u5de5\u4f5c\u533a"),
        uiText(u"\u591a\u5de5\u51b5\u6570\u636e\u3001\u516d\u5206\u91cf\u6570\u636e\u4e0e\u5206\u6790\u4efb\u52a1\u7684\u754c\u9762\u5165\u53e3\u3002\u672c\u9636\u6bb5\u4e0d\u6267\u884c\u8235\u6548\u6216\u6c14\u52a8\u529b\u5206\u6790\u7b97\u6cd5\u3002"));
    qobject_cast<QVBoxLayout *>(dataProcessPage_->layout())->addStretch();
    for (auto *page : {stateListPage_, batchPlanPage_, batchMonitorPage_, taskMonitorPage_, dataProcessPage_})
        workspaceStack_->addWidget(page);
    setCentralWidget(workspaceStack_);

    projectDock_ = new QDockWidget(uiText(u"\u6a21\u5757\u5bfc\u822a"), this);
    projectDock_->setObjectName(QStringLiteral("projectDock"));
    projectDock_->setMinimumWidth(220);
    navigationStack_ = new QStackedWidget(projectDock_);
    navigationStack_->setObjectName(QStringLiteral("navigationStack"));
    projectTree_ = new QTreeWidget(navigationStack_);
    projectTree_->setObjectName(QStringLiteral("projectTree"));
    projectTree_->setHeaderHidden(true);
    auto *root = new QTreeWidgetItem(projectTree_, {uiText(u"\u5de5\u7a0b Project\uff08\u754c\u9762\u793a\u4f8b\uff0c\u672a\u6253\u5f00\u5de5\u7a0b\uff09")});
    auto *mesh = addNavigationItem(root, uiText(u"\u7f51\u683c Mesh"), QStringLiteral("mesh"), {}, QStringLiteral("mesh"));
    addNavigationItem(mesh, uiText(u"\u5df2\u5bfc\u5165\u7f51\u683c\uff08\u672a\u52a0\u8f7d\uff09"), QStringLiteral("mesh"), QStringLiteral("mesh-placeholder"), QStringLiteral("mesh"));
    addNavigationItem(root, uiText(u"\u7269\u7406\u6a21\u578b Physics"), QStringLiteral("physics"), {}, QStringLiteral("physics"));
    addNavigationItem(root, uiText(u"\u8fd0\u52a8 Motion"), QStringLiteral("motion"), {}, QStringLiteral("motion"));
    addNavigationItem(root, uiText(u"\u8fb9\u754c\u6761\u4ef6"), QStringLiteral("boundary"), {}, QStringLiteral("boundary"));
    addNavigationItem(root, uiText(u"\u8ba1\u7b97\u53c2\u6570"), QStringLiteral("solver"), {}, QStringLiteral("solver"));
    addNavigationItem(root, uiText(u"\u5e76\u884c\u8ba1\u7b97"), QStringLiteral("parallel"), {}, QStringLiteral("parallel"));
    auto *states = addNavigationItem(root, uiText(u"\u5de5\u51b5\u7ba1\u7406"), QStringLiteral("state-manager"), QStringLiteral("states"), QStringLiteral("state-manager"));
    for (int i = 1; i <= 3; ++i) {
        const QString id = QStringLiteral("State_%1").arg(i, 3, 10, QLatin1Char('0'));
        addNavigationItem(states, id + uiText(u"\uff08\u754c\u9762\u793a\u4f8b\uff09"), QStringLiteral("state"), id, QStringLiteral("state"));
    }
    auto *plans = addNavigationItem(root, uiText(u"\u6279\u5904\u7406\u8ba1\u5212"), QStringLiteral("batch-plans"), QStringLiteral("batch-plans"), QStringLiteral("batch-plans"));
    for (int i = 1; i <= 2; ++i) {
        const QString id = QStringLiteral("Plan_%1").arg(i, 3, 10, QLatin1Char('0'));
        addNavigationItem(plans, id + uiText(u"\uff08\u754c\u9762\u793a\u4f8b\uff09"), QStringLiteral("batch-plan"), id, QStringLiteral("batch-plan"));
    }
    addNavigationItem(root, uiText(u"\u7ed3\u679c Results"), QStringLiteral("results"), {}, QStringLiteral("results"));
    root->setExpanded(true);
    connect(projectTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) { handleNavigationSelection(item); });
    navigationStack_->addWidget(projectTree_);

    monitorTree_ = new QTreeWidget(navigationStack_);
    monitorTree_->setObjectName(QStringLiteral("monitorTree"));
    monitorTree_->setHeaderHidden(true);
    auto *monitorRoot = new QTreeWidgetItem(monitorTree_, {uiText(u"\u5de5\u7a0b Project\uff08\u754c\u9762\u793a\u4f8b\uff09")});
    auto *calculationTasks = addNavigationItem(monitorRoot, uiText(u"\u8ba1\u7b97\u4efb\u52a1"), {}, QStringLiteral("calculation-tasks"), QStringLiteral("group"));
    auto *singleRuns = addNavigationItem(calculationTasks, uiText(u"\u5355\u6b21\u8ba1\u7b97"), {}, QStringLiteral("single-runs"), QStringLiteral("group"));
    for (int i = 1; i <= 2; ++i) {
        const QString id = QStringLiteral("Run_%1").arg(i, 3, 10, QLatin1Char('0'));
        auto *run = addNavigationItem(singleRuns, id + uiText(u"\uff08Mock \u793a\u4f8b\uff09"), QStringLiteral("task"), id, QStringLiteral("task"));
        setTaskItemStatus(run, i == 1 ? TaskStatus::Running : TaskStatus::Finished);
    }
    auto *batchRuns = addNavigationItem(calculationTasks, uiText(u"\u6279\u5904\u7406\u8ba1\u7b97"), {}, QStringLiteral("batch-runs"), QStringLiteral("group"));
    auto *batchRun = addNavigationItem(batchRuns, uiText(u"BatchRun_001\uff08Mock \u793a\u4f8b\uff09"), QStringLiteral("batch-run"), QStringLiteral("BatchRun_001"), QStringLiteral("batch-run"));
    setTaskItemStatus(batchRun, TaskStatus::Running);
    for (int i = 1; i <= 3; ++i) {
        const QString id = QStringLiteral("Task_%1").arg(i, 3, 10, QLatin1Char('0'));
        auto *task = addNavigationItem(batchRun, id + uiText(u"\uff08Mock \u793a\u4f8b\uff09"), QStringLiteral("task"), id, QStringLiteral("task"));
        setTaskItemStatus(task, i == 1 ? TaskStatus::Finished : i == 2 ? TaskStatus::Running : TaskStatus::Preparing);
    }
    auto *monitorViews = addNavigationItem(monitorRoot, uiText(u"\u76d1\u63a7\u89c6\u56fe"), {}, QStringLiteral("monitor-views"), QStringLiteral("group"));
    addNavigationItem(monitorViews, uiText(u"\u8ba1\u7b97\u65e5\u5fd7"), QStringLiteral("monitor-log"), QStringLiteral("log"), QStringLiteral("monitor-view"));
    addNavigationItem(monitorViews, uiText(u"\u6536\u655b\u66f2\u7ebf"), QStringLiteral("monitor-residual"), QStringLiteral("residual"), QStringLiteral("monitor-view"));
    addNavigationItem(monitorViews, uiText(u"\u9519\u8bef\u4fe1\u606f"), QStringLiteral("monitor-errors"), QStringLiteral("errors"), QStringLiteral("monitor-view"));
    monitorRoot->setExpanded(true);
    calculationTasks->setExpanded(true);
    connect(monitorTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) { handleNavigationSelection(item); });
    navigationStack_->addWidget(monitorTree_);

    dataTree_ = new QTreeWidget(navigationStack_);
    dataTree_->setObjectName(QStringLiteral("dataProcessTree"));
    dataTree_->setHeaderHidden(true);
    auto *dataRoot = new QTreeWidgetItem(dataTree_, {uiText(u"\u5de5\u7a0b Project\uff08\u754c\u9762\u793a\u4f8b\uff09")});
    auto *datasets = addNavigationItem(dataRoot, uiText(u"\u591a\u5de5\u51b5\u6570\u636e\u96c6"), QStringLiteral("data-process"), QStringLiteral("datasets"), QStringLiteral("data-group"));
    for (int i = 1; i <= 2; ++i) {
        const QString id = QStringLiteral("Dataset_%1").arg(i, 3, 10, QLatin1Char('0'));
        addNavigationItem(datasets, id + uiText(u"\uff08\u754c\u9762\u793a\u4f8b\uff09"), QStringLiteral("dataset"), id, QStringLiteral("dataset"));
    }
    addNavigationItem(dataRoot, uiText(u"\u516d\u5206\u91cf\u6570\u636e"), QStringLiteral("data-process"), QStringLiteral("six-component"), QStringLiteral("data-source"));
    auto *methods = addNavigationItem(dataRoot, uiText(u"\u6570\u636e\u5904\u7406\u65b9\u6cd5"), QStringLiteral("data-process"), QStringLiteral("methods"), QStringLiteral("data-group"));
    addNavigationItem(methods, uiText(u"\u8235\u6548\u5206\u6790\uff08\u672a\u5b9e\u73b0\uff09"), QStringLiteral("data-process"), QStringLiteral("control-effectiveness"), QStringLiteral("data-method"));
    addNavigationItem(methods, uiText(u"\u6c14\u52a8\u529b\u5206\u6790\uff08\u672a\u5b9e\u73b0\uff09"), QStringLiteral("data-process"), QStringLiteral("aerodynamics"), QStringLiteral("data-method"));
    addNavigationItem(dataRoot, uiText(u"\u5206\u6790\u4efb\u52a1"), QStringLiteral("data-process"), QStringLiteral("analysis-tasks"), QStringLiteral("data-group"));
    addNavigationItem(dataRoot, uiText(u"\u5904\u7406\u7ed3\u679c"), QStringLiteral("data-process"), QStringLiteral("analysis-results"), QStringLiteral("data-group"));
    dataRoot->setExpanded(true);
    datasets->setExpanded(true);
    connect(dataTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) { handleNavigationSelection(item); });
    navigationStack_->addWidget(dataTree_);
    navigationStack_->setCurrentWidget(projectTree_);
    projectDock_->setWidget(navigationStack_);

    // Workflow data remains available to run gating and services, but no longer
    // consumes the navigation area required by the three module trees.
    suggestion_ = new QLabel(uiText(u"\u7b49\u5f85\u5de5\u4f5c\u6d41\u670d\u52a1"), this);
    suggestion_->hide();
    workflowTree_ = new QTreeWidget(this);
    workflowTree_->setObjectName(QStringLiteral("workflowTree"));
    workflowTree_->setHeaderLabels({uiText(u"\u6b65\u9aa4 / \u64cd\u4f5c"), uiText(u"\u72b6\u6001")});
    workflowTree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    workflowTree_->hide();
    connect(workflowTree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const QString page = item->data(0, Qt::UserRole).toString();
        if (!page.isEmpty()) selectPage(page);
        emit navigationRequested(page, item->data(0, Qt::UserRole + 1).toString(), item->data(0, Qt::UserRole + 2).toString());
    });
    propertyDock_ = new QDockWidget(uiText(u"\u5c5e\u6027\u7f16\u8f91\u5668"), this);
    propertyDock_->setObjectName(QStringLiteral("propertyDock"));
    propertyDock_->setMinimumWidth(300);
    auto *properties = new QWidget(propertyDock_);
    auto *propertyLayout = new QVBoxLayout(properties);
    propertyTitle_ = new QLabel(properties);
    propertyTitle_->setObjectName(QStringLiteral("propertyTitle"));
    propertyLayout->addWidget(propertyTitle_);
    auto *notice = new QLabel(uiText(u"\u53c2\u6570\u8349\u7a3f\u793a\u4f8b \u00b7 \u4ec5\u4fdd\u5b58\u5728\u5f53\u524d\u7a97\u53e3\n\u672a\u6253\u5f00\u771f\u5b9e\u5de5\u7a0b\u65f6\u7f16\u8f91\u63a7\u4ef6\u9501\u5b9a"), properties);
    notice->setWordWrap(true);
    propertyLayout->addWidget(notice);
    propertyForm_ = new QFormLayout;
    propertyForm_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    propertyLayout->addLayout(propertyForm_);
    propertyLayout->addStretch();
    draftReset_ = new QPushButton(uiText(u"\u6e05\u9664\u672c\u9875\u8349\u7a3f"), properties);
    draftReset_->setEnabled(false);
    connect(draftReset_, &QPushButton::clicked, this, [this] { drafts_.remove(currentPage_); selectPage(currentPage_); });
    propertyLayout->addWidget(draftReset_);
    propertyDock_->setWidget(properties);
    selectPage(QStringLiteral("mesh"));

    // Reuse the existing monitor slot and task list in the right-hand monitor
    // workspaces. The bottom dock is reserved for unified runtime output.
    auto *taskMonitorLayout = qobject_cast<QVBoxLayout *>(taskMonitorPage_->layout());
    taskMonitorTitle_ = new QLabel(uiText(u"Task_001 \u00b7 \u8fd0\u884c\u4e2d\uff08Mock\uff09"), taskMonitorPage_);
    taskMonitorTitle_->setObjectName(QStringLiteral("taskMonitorTitle"));
    QFont taskTitleFont = taskMonitorTitle_->font();
    taskTitleFont.setPointSize(taskTitleFont.pointSize() + 2);
    taskTitleFont.setBold(true);
    taskMonitorTitle_->setFont(taskTitleFont);
    taskMonitorLayout->addWidget(taskMonitorTitle_);
    auto *monitorSlot = createSlot(WidgetSlot::Monitor, {});
    monitorSlot->setObjectName(QStringLiteral("monitorWorkspaceSlot"));
    auto *monitorLayout = slotLayouts_.value(static_cast<int>(WidgetSlot::Monitor));
    auto *oldMonitorItem = monitorLayout->takeAt(0);
    delete oldMonitorItem->widget();
    delete oldMonitorItem;
    auto *monitor = new QFrame(monitorSlot);
    monitor->setObjectName(QStringLiteral("taskMetricsFrame"));
    monitor->setFrameShape(QFrame::StyledPanel);
    auto *monitorForm = new QFormLayout(monitor);
    monitorStatus_ = new QLabel(uiText(u"Running\uff08Mock\uff09"), monitor);
    monitorDetails_ = new QLabel(uiText(u"\u5f53\u524d\u8fed\u4ee3\uff1a1250    \u603b\u8fed\u4ee3\uff1a10000    \u6b8b\u5dee\uff1a1.2e-4\uff08Mock\uff09"), monitor);
    progress_ = new QProgressBar(monitor);
    progress_->setRange(0, 1000);
    progress_->setValue(125);
    progress_->setFormat(uiText(u"12.5%\uff08Mock\uff09"));
    monitorForm->addRow(uiText(u"\u8fd0\u884c\u72b6\u6001"), monitorStatus_);
    monitorForm->addRow(uiText(u"\u8fed\u4ee3\u4fe1\u606f"), monitorDetails_);
    monitorForm->addRow(uiText(u"\u8ba1\u7b97\u8fdb\u5ea6"), progress_);
    monitorLayout->addWidget(monitor);
    taskMonitorLayout->addWidget(monitorSlot);
    auto *curveFrame = new QFrame(taskMonitorPage_);
    curveFrame->setObjectName(QStringLiteral("convergenceCurvePanel"));
    curveFrame->setFrameShape(QFrame::StyledPanel);
    curveFrame->setMinimumHeight(150);
    auto *curveLayout = new QVBoxLayout(curveFrame);
    auto *curveTitle = new QLabel(uiText(u"\u6536\u655b\u66f2\u7ebf"), curveFrame);
    QFont curveTitleFont = curveTitle->font();
    curveTitleFont.setBold(true);
    curveTitle->setFont(curveTitleFont);
    curveLayout->addWidget(curveTitle);
    curveLayout->addWidget(emptyState(uiText(u"Mock \u66f2\u7ebf\u533a\u57df\n\u7b49\u5f85\u6b8b\u5dee\u91c7\u6837\u6570\u636e\u4e0e\u66f2\u7ebf\u7ec4\u4ef6\u63a5\u5165"), curveFrame), 1);
    taskMonitorLayout->addWidget(curveFrame, 1);
    auto *resourceSummary = new QLabel(uiText(u"CPU 76%    \u5185\u5b58 18.3 GB    MPI 32    \u8ba1\u7b97\u65f6\u95f4 00:12:35    \uff08Mock\uff09"), taskMonitorPage_);
    resourceSummary->setObjectName(QStringLiteral("taskResourceSummary"));
    taskMonitorLayout->addWidget(resourceSummary);

    taskTree_ = new QTreeWidget(batchMonitorPage_);
    taskTree_->setObjectName(QStringLiteral("taskTree"));
    taskTree_->setHeaderLabels({uiText(u"\u4efb\u52a1 ID"), uiText(u"\u72b6\u6001"), uiText(u"\u6d88\u606f"), uiText(u"\u65f6\u95f4")});
    taskTree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    taskTree_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    taskTree_->setAlternatingRowColors(true);
    new QTreeWidgetItem(taskTree_, {QStringLiteral("Task_001"), uiText(u"\u5df2\u5b8c\u6210\uff08Mock\uff09"), uiText(u"\u793a\u4f8b\u5b50\u4efb\u52a1"), QStringLiteral("00:08:21")});
    new QTreeWidgetItem(taskTree_, {QStringLiteral("Task_002"), uiText(u"\u8fd0\u884c\u4e2d\uff08Mock\uff09"), uiText(u"\u793a\u4f8b\u5b50\u4efb\u52a1"), QStringLiteral("00:04:10")});
    new QTreeWidgetItem(taskTree_, {QStringLiteral("Task_003"), uiText(u"\u51c6\u5907\u4e2d\uff08Mock\uff09"), uiText(u"\u793a\u4f8b\u5b50\u4efb\u52a1"), QStringLiteral("--")});
    connect(taskTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (item) selectNavigationNode(monitorTree_, item->text(0));
    });
    batchMonitorLayout->insertWidget(batchMonitorLayout->count() - 1, taskTree_, 1);

    outputDock_ = new QDockWidget(uiText(u"\u8f93\u51fa"), this);
    outputDock_->setObjectName(QStringLiteral("outputDock"));
    auto *outputTitleBar = new QWidget(outputDock_);
    outputTitleBar->setObjectName(QStringLiteral("outputToolbar"));
    auto *outputTools = new QHBoxLayout(outputTitleBar);
    outputTools->setContentsMargins(8, 3, 6, 3);
    outputTools->setSpacing(6);
    outputTitle_ = new QLabel(outputTitleBar);
    outputTitle_->setObjectName(QStringLiteral("outputTitle"));
    QFont outputTitleFont = outputTitle_->font();
    outputTitleFont.setBold(true);
    outputTitle_->setFont(outputTitleFont);
    outputTools->addWidget(outputTitle_);
    logLevelFilter_ = new QComboBox(outputTitleBar);
    logLevelFilter_->setObjectName(QStringLiteral("logLevelFilter"));
    logLevelFilter_->addItem(uiText(u"\u5168\u90e8\u7ea7\u522b"), QStringLiteral("ALL"));
    for (const QString &level : {QStringLiteral("INFO"), QStringLiteral("WARN"), QStringLiteral("ERROR"), QStringLiteral("DEBUG")})
        logLevelFilter_->addItem(level, level);
    logLevelFilter_->setMaximumWidth(120);
    outputTools->addWidget(logLevelFilter_);
    logModuleFilter_ = new QComboBox(outputTitleBar);
    logModuleFilter_->setObjectName(QStringLiteral("logModuleFilter"));
    logModuleFilter_->addItem(uiText(u"\u5168\u90e8\u6a21\u5757"), QStringLiteral("ALL"));
    const QList<QPair<QString, QString>> modules = {
        {uiText(u"\u754c\u9762"), QStringLiteral("UI")}, {uiText(u"\u5de5\u7a0b"), QStringLiteral("Project")},
        {uiText(u"\u7f51\u683c"), QStringLiteral("Mesh")}, {uiText(u"\u6821\u9a8c"), QStringLiteral("Validation")},
        {uiText(u"\u6c42\u89e3\u5668"), QStringLiteral("Solver")}, {uiText(u"\u4efb\u52a1"), QStringLiteral("Task")},
        {uiText(u"\u6279\u5904\u7406"), QStringLiteral("Batch")}};
    for (const auto &module : modules) logModuleFilter_->addItem(module.first, module.second);
    logModuleFilter_->setMaximumWidth(140);
    outputTools->addWidget(logModuleFilter_);
    logSearch_ = new QLineEdit(outputTitleBar);
    logSearch_->setObjectName(QStringLiteral("logSearch"));
    logSearch_->setPlaceholderText(uiText(u"\u641c\u7d22\u65e5\u5fd7"));
    logSearch_->setClearButtonEnabled(true);
    logSearch_->setMaximumWidth(220);
    outputTools->addWidget(logSearch_, 1);
    outputTools->addStretch();
    outputScopeReset_ = new QPushButton(uiText(u"\u5168\u90e8\u65e5\u5fd7"), outputTitleBar);
    outputScopeReset_->setObjectName(QStringLiteral("outputScopeReset"));
    outputScopeReset_->setToolTip(uiText(u"\u6e05\u9664\u5f53\u524d\u4efb\u52a1\u8303\u56f4\u7b5b\u9009"));
    outputScopeReset_->setMaximumWidth(90);
    outputTools->addWidget(outputScopeReset_);
    auto *clearOutput = new QPushButton(uiText(u"\u6e05\u7a7a"), outputTitleBar);
    clearOutput->setObjectName(QStringLiteral("clearOutputButton"));
    clearOutput->setToolTip(uiText(u"\u4ec5\u6e05\u7a7a\u5f53\u524d\u754c\u9762\u65e5\u5fd7\uff0c\u4e0d\u5220\u9664\u6587\u4ef6"));
    clearOutput->setMaximumWidth(60);
    outputTools->addWidget(clearOutput);
    auto *exportOutput = new QPushButton(uiText(u"\u5bfc\u51fa"), outputTitleBar);
    exportOutput->setObjectName(QStringLiteral("exportOutputButton"));
    exportOutput->setEnabled(false);
    exportOutput->setToolTip(uiText(u"\u65e5\u5fd7\u5bfc\u51fa\u5c1a\u672a\u63a5\u5165"));
    exportOutput->setMaximumWidth(60);
    outputTools->addWidget(exportOutput);
    logAutoScroll_ = new QCheckBox(uiText(u"\u81ea\u52a8\u6eda\u52a8"), outputTitleBar);
    logAutoScroll_->setObjectName(QStringLiteral("logAutoScroll"));
    logAutoScroll_->setChecked(true);
    outputTools->addWidget(logAutoScroll_);
    outputCollapse_ = new QPushButton(uiText(u"\u6536\u8d77"), outputTitleBar);
    outputCollapse_->setObjectName(QStringLiteral("outputCollapseButton"));
    outputCollapse_->setMaximumWidth(60);
    outputTools->addWidget(outputCollapse_);
    outputDock_->setTitleBarWidget(outputTitleBar);

    auto *logSlot = createSlot(WidgetSlot::Log, {});
    logSlot->setObjectName(QStringLiteral("outputBody"));
    outputBody_ = logSlot;
    auto *logLayout = slotLayouts_.value(static_cast<int>(WidgetSlot::Log));
    auto *oldLogItem = logLayout->takeAt(0);
    delete oldLogItem->widget();
    delete oldLogItem;
    outputLogTable_ = new QTableWidget(0, 4, logSlot);
    outputLogTable_->setObjectName(QStringLiteral("outputLogTable"));
    outputLogTable_->setHorizontalHeaderLabels({uiText(u"\u65f6\u95f4"), uiText(u"\u7ea7\u522b"), uiText(u"\u6765\u6e90"), uiText(u"\u65e5\u5fd7\u6d88\u606f")});
    outputLogTable_->verticalHeader()->hide();
    outputLogTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    outputLogTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    outputLogTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    outputLogTable_->setShowGrid(false);
    outputLogTable_->setAlternatingRowColors(true);
    outputLogTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    outputLogTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    outputLogTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    outputLogTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    auto *copyLogs = new QAction(outputLogTable_);
    copyLogs->setShortcut(QKeySequence::Copy);
    copyLogs->setShortcutContext(Qt::WidgetShortcut);
    outputLogTable_->addAction(copyLogs);
    connect(copyLogs, &QAction::triggered, this, [this] {
        const QModelIndexList indexes = outputLogTable_->selectionModel()->selectedIndexes();
        QString copied;
        int previousRow = -1;
        for (const auto &index : indexes) {
            if (previousRow >= 0) copied += index.row() == previousRow ? QLatin1Char('\t') : QLatin1Char('\n');
            copied += index.data().toString();
            previousRow = index.row();
        }
        QApplication::clipboard()->setText(copied);
    });
    logLayout->addWidget(outputLogTable_);
    outputDock_->setWidget(logSlot);

    // Preserve the legacy Error slot for compatible adapters without exposing
    // a second bottom tab. New messages are rendered by the unified log table.
    auto *errorSlot = createSlot(WidgetSlot::Error, {});
    errorSlot->setObjectName(QStringLiteral("legacyErrorSlot"));
    errorSlot->hide();

    connect(logLevelFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { applyLogFilters(); });
    connect(logModuleFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { applyLogFilters(); });
    connect(logSearch_, &QLineEdit::textChanged, this, [this] { applyLogFilters(); });
    connect(outputScopeReset_, &QPushButton::clicked, this, [this] { setOutputScope({}); });
    connect(clearOutput, &QPushButton::clicked, this, [this] { outputLogTable_->setRowCount(0); });
    connect(outputCollapse_, &QPushButton::clicked, this, [this] { setOutputCollapsed(!outputCollapsed_); });
    appendLog(QStringLiteral("INFO"), QStringLiteral("Project"), uiText(u"Mock\uff1a\u5de5\u7a0b DemoProject \u5df2\u6253\u5f00"), QStringLiteral("current-project"));
    appendLog(QStringLiteral("INFO"), QStringLiteral("Mesh"), uiText(u"Mock\uff1a\u7f51\u683c\u6587\u4ef6\u52a0\u8f7d\u5b8c\u6210"), QStringLiteral("current-project"));
    appendLog(QStringLiteral("WARN"), QStringLiteral("Validation"), uiText(u"Mock\uff1a\u5de5\u51b5\u53c2\u6570\u5c1a\u672a\u5b8c\u6210\u6821\u9a8c"), QStringLiteral("current-project"));
    appendLog(QStringLiteral("ERROR"), QStringLiteral("Solver"), uiText(u"Mock\uff1a\u6c42\u89e3\u5668\u8f93\u5165\u6587\u4ef6\u4e0d\u5b58\u5728"), QStringLiteral("current-project"));
    appendLog(QStringLiteral("INFO"), QStringLiteral("Task"), uiText(u"Mock\uff1aTask_001 \u5df2\u542f\u52a8"), QStringLiteral("Task_001"));
    appendLog(QStringLiteral("WARN"), QStringLiteral("Task"), uiText(u"Mock\uff1aTask_001 \u6536\u655b\u901f\u5ea6\u4f4e\u4e8e\u9884\u671f"), QStringLiteral("Task_001"));
    appendLog(QStringLiteral("INFO"), QStringLiteral("Batch"), uiText(u"Mock\uff1aBatchRun_001 \u5df2\u521b\u5efa 3 \u4e2a\u5b50\u4efb\u52a1"), QStringLiteral("BatchRun_001"));
    appendLog(QStringLiteral("DEBUG"), QStringLiteral("Task"), uiText(u"Mock\uff1aTask_002 \u7b49\u5f85\u8d44\u6e90"), QStringLiteral("Task_002"));
    setOutputScope(QStringLiteral("current-project"));
}

void MainWindow::switchNavigationForCategory(int index)
{
    auto *category = ribbonBar()->categoryByIndex(index);
    if (!category) return;
    if (category->objectName() == QStringLiteral("startCategory")) {
        navigationStack_->setCurrentWidget(projectTree_);
        showWorkspacePage(QStringLiteral("viewport"));
        setOutputScope(QStringLiteral("current-project"));
    } else if (category->objectName() == QStringLiteral("monitorCategory")) {
        navigationStack_->setCurrentWidget(monitorTree_);
        auto *selected = monitorTree_->currentItem();
        const QString page = selected ? selected->data(0, PageRole).toString() : QString();
        if (page == QStringLiteral("batch-run"))
            showWorkspacePage(QStringLiteral("batch-monitor"));
        else
            showWorkspacePage(QStringLiteral("task-monitor"));
    } else if (category->objectName() == QStringLiteral("dataCategory"))
        navigationStack_->setCurrentWidget(dataTree_);
    // Postprocess and Help intentionally retain the previous navigation tree.
}

void MainWindow::handleNavigationSelection(QTreeWidgetItem *item)
{
    if (!item) return;
    const QString page = item->data(0, PageRole).toString();
    if (page.isEmpty()) return;
    const QString objectId = item->data(0, ObjectIdRole).toString();
    const QString draftPage = objectId.isEmpty() ? page : page + QLatin1Char('|') + objectId;
    selectPage(draftPage);
    emit navigationRequested(page, objectId, {});
    if (item->treeWidget() == projectTree_) setOutputScope(QStringLiteral("current-project"));
    if (page == QStringLiteral("state-manager")) showWorkspacePage(QStringLiteral("states"));
    else if (page == QStringLiteral("batch-plans") || page == QStringLiteral("batch-plan")) showWorkspacePage(QStringLiteral("batch-plan"));
    else if (page == QStringLiteral("batch-run")) {
        showWorkspacePage(QStringLiteral("batch-monitor"));
        setOutputScope(objectId);
    } else if (page == QStringLiteral("task")) {
        showWorkspacePage(QStringLiteral("task-monitor"));
        setOutputScope(objectId);
        if (taskMonitorTitle_) taskMonitorTitle_->setText(objectId + uiText(u" \u00b7 \u7efc\u5408\u76d1\u63a7\uff08Mock\uff09"));
    }
    else if (page == QStringLiteral("dataset") || page == QStringLiteral("data-process")) showWorkspacePage(QStringLiteral("data-process"));
    else if (page == QStringLiteral("monitor-log")) showOutput(0);
    else if (page == QStringLiteral("monitor-residual")) showWorkspacePage(QStringLiteral("task-monitor"));
    else if (page == QStringLiteral("monitor-errors")) showOutput(3);
    else showWorkspacePage(QStringLiteral("viewport"));
}

void MainWindow::showWorkspacePage(const QString &pageId)
{
    QWidget *target = viewportPage_;
    if (pageId == QStringLiteral("states")) target = stateListPage_;
    else if (pageId == QStringLiteral("batch-plan")) target = batchPlanPage_;
    else if (pageId == QStringLiteral("batch-monitor")) target = batchMonitorPage_;
    else if (pageId == QStringLiteral("task-monitor")) target = taskMonitorPage_;
    else if (pageId == QStringLiteral("data-process")) target = dataProcessPage_;
    workspaceStack_->setCurrentWidget(target);
}

void MainWindow::selectNavigationNode(QTreeWidget *tree, const QString &objectId)
{
    if (!tree) return;
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, ObjectIdRole).toString() != objectId) continue;
        for (auto *parent = (*it)->parent(); parent; parent = parent->parent()) parent->setExpanded(true);
        tree->setCurrentItem(*it);
        tree->scrollToItem(*it);
        return;
    }
}

void MainWindow::selectPage(const QString &pageId)
{
    currentPage_ = pageId;
    const QString pageType = pageId.section(QLatin1Char('|'), 0, 0);
    const QString objectId = pageId.section(QLatin1Char('|'), 1, 1);
    while (propertyForm_->rowCount()) propertyForm_->removeRow(0);
    const QHash<QString, QString> titles = {{"mesh", uiText(u"\u7f51\u683c\u4e0e\u53d8\u6362")}, {"physics", uiText(u"\u7269\u7406\u6a21\u578b / \u5927\u6c14\u53c2\u6570")},
        {"motion", uiText(u"\u8fd0\u52a8\u53c2\u6570")}, {"boundary", uiText(u"\u8fb9\u754c\u6761\u4ef6")}, {"solver", uiText(u"\u8ba1\u7b97\u53c2\u6570")},
        {"parallel", uiText(u"\u5e76\u884c\u8bbe\u7f6e")}, {"states", uiText(u"\u8ba1\u7b97\u72b6\u6001 \u00b7 \u4e00\u6b21\u8ba1\u7b97\u4e00\u4e2a\u72b6\u6001")},
        {"state-manager", uiText(u"\u5de5\u51b5\u7ba1\u7406")}, {"state", uiText(u"\u5de5\u51b5\u53c2\u6570")},
        {"batch-plans", uiText(u"\u6279\u5904\u7406\u8ba1\u5212\u7ba1\u7406")}, {"batch-plan", uiText(u"\u6279\u5904\u7406\u8ba1\u5212\u5c5e\u6027")},
        {"batch-run", uiText(u"\u6279\u5904\u7406\u8fd0\u884c\u4fe1\u606f")}, {"task", uiText(u"\u8ba1\u7b97\u4efb\u52a1\u4fe1\u606f")},
        {"dataset", uiText(u"\u6570\u636e\u96c6\u4fe1\u606f")}, {"data-process", uiText(u"\u6570\u636e\u5904\u7406")},
        {"monitor-log", uiText(u"\u8ba1\u7b97\u65e5\u5fd7")}, {"monitor-residual", uiText(u"\u6536\u655b\u66f2\u7ebf")},
        {"monitor-errors", uiText(u"\u9519\u8bef\u4fe1\u606f")}, {"results", uiText(u"\u7ed3\u679c\u4e0e\u540e\u5904\u7406")}};
    propertyTitle_->setText(titles.value(pageType, pageType) + (objectId.isEmpty() ? QString() : uiText(u" \u00b7 ") + objectId));
    const auto addNumber = [this, pageId](const QString &key, const QString &label, double value, const QString &unit) {
        auto *editor = new QDoubleSpinBox(this);
        editor->setObjectName(key);
        editor->setDecimals(4);
        editor->setRange(-1e9, 1e9);
        editor->setSuffix(unit);
        editor->setValue(drafts_.value(pageId).value(key, value).toDouble());
        editor->setEnabled(projectEditingEnabled_);
        if (!projectEditingEnabled_) editor->setToolTip(uiText(u"\u8bf7\u5148\u901a\u8fc7\u5de5\u7a0b\u670d\u52a1\u6253\u5f00\u771f\u5b9e\u5de5\u7a0b"));
        propertyForm_->addRow(label, editor);
        connect(editor, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, pageId, key](double value) {
            drafts_[pageId].insert(key, value);
            emit parameterDraftChanged(pageId, drafts_.value(pageId));
        });
    };
    if (pageType == "boundary" || pageType == "physics") {
        auto *type = new QComboBox(this);
        type->addItems({uiText(u"\u538b\u529b\u5165\u53e3"), uiText(u"\u901f\u5ea6\u5165\u53e3"), uiText(u"\u538b\u529b\u51fa\u53e3"), uiText(u"\u58c1\u9762")});
        type->setCurrentIndex(drafts_.value(pageId).value("type", 0).toInt());
        type->setEnabled(projectEditingEnabled_);
        propertyForm_->addRow(uiText(u"\u8fb9\u754c\u7c7b\u578b\uff08\u793a\u4f8b\uff09"), type);
        connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, pageId](int value) {
            drafts_[pageId].insert("type", value);
            emit parameterDraftChanged(pageId, drafts_.value(pageId));
        });
        addNumber("pressure", uiText(u"\u538b\u529b"), 101325, " Pa");
        addNumber("temperature", uiText(u"\u6e29\u5ea6"), 300, " K");
    } else if (pageType == "motion" || pageType == "mesh") {
        if (pageType == "motion") addNumber("mass", uiText(u"\u8d28\u91cf"), 15.2, " kg");
        for (const QString &axis : {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")})
            addNumber(axis, uiText(u"\u4f4d\u7f6e ") + axis, 0, " m");
    } else if (pageType == "solver") {
        addNumber("iterations", uiText(u"\u6700\u5927\u8fed\u4ee3\u6b65"), 10000, {});
        addNumber("timeStep", uiText(u"\u65f6\u95f4\u6b65\u957f"), 0.001, " s");
    } else if (pageType == "parallel") {
        addNumber("processes", uiText(u"MPI \u8fdb\u7a0b\u6570"), 32, {});
    } else if (pageType == "state") {
        addNumber("altitude", uiText(u"\u9ad8\u5ea6\uff08\u754c\u9762\u8349\u7a3f\uff09"), 5000, " m");
        addNumber("mach", uiText(u"\u9a6c\u8d6b\u6570\uff08\u754c\u9762\u8349\u7a3f\uff09"), 0.8, {});
    } else if (pageType == "batch-plan") {
        const QList<QPair<QString, QString>> values = {
            {uiText(u"\u8ba1\u5212 ID"), objectId}, {uiText(u"\u7248\u672c"), QStringLiteral("1 (Mock)")},
            {uiText(u"\u6267\u884c\u7b56\u7565"), uiText(u"\u987a\u5e8f\u6267\u884c\uff08\u754c\u9762\u793a\u4f8b\uff09")},
            {uiText(u"\u5de5\u51b5\u6570\u91cf"), QStringLiteral("3")}, {uiText(u"\u6821\u9a8c\u72b6\u6001"), uiText(u"\u5f85\u4e1a\u52a1\u670d\u52a1\u786e\u8ba4")}};
        for (const auto &value : values) propertyForm_->addRow(value.first, new QLabel(value.second, this));
    } else if (pageType == "batch-run" || pageType == "task" || pageType == "dataset" || pageType == "data-process") {
        auto *note = new QLabel(uiText(u"\u5bf9\u8c61\uff1a%1\n\u5f53\u524d\u4ec5\u5c55\u793a Mock / \u5360\u4f4d\u4fe1\u606f\uff0c\u5c1a\u672a\u4ece\u5de5\u7a0b\u3001\u4efb\u52a1\u6216\u6570\u636e\u670d\u52a1\u52a0\u8f7d\u3002").arg(objectId), this);
        note->setWordWrap(true);
        propertyForm_->addRow(note);
    } else {
        auto *note = new QLabel(pageType == "results" ? uiText(u"\u5c1a\u65e0\u7ed3\u679c\u6570\u636e\u3002\u7ed3\u679c\u5339\u914d\u6821\u9a8c\u548c\u663e\u793a\u7531\u5de5\u4f5c\u6d41\u53ca\u53ef\u89c6\u5316\u6a21\u5757\u63d0\u4f9b\u3002") : uiText(u"\u4e00\u6b21\u8ba1\u7b97\u5bf9\u5e94\u4e00\u4e2a\u5de5\u51b5\uff0c\u5de5\u51b5\u5f52\u5c5e\u5f53\u524d\u5de5\u7a0b\u3002\u6279\u5904\u7406\u8ba1\u5212\u53ea\u5f15\u7528\u5de5\u51b5\uff0c\u5f53\u524d\u4ec5\u4e3a\u754c\u9762\u793a\u4f8b\u3002"), this);
        note->setWordWrap(true);
        propertyForm_->addRow(note);
    }
    propertyDock_->show();
}

void MainWindow::restoreLayout()
{
    for (auto *dock : {projectDock_, propertyDock_, outputDock_}) {
        dock->setFloating(false);
        removeDockWidget(dock);
    }
    addDockWidget(Qt::LeftDockWidgetArea, projectDock_);
    addDockWidget(Qt::LeftDockWidgetArea, propertyDock_);
    splitDockWidget(projectDock_, propertyDock_, Qt::Horizontal);
    addDockWidget(Qt::BottomDockWidgetArea, outputDock_);
    setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
    for (auto *dock : {projectDock_, propertyDock_, outputDock_}) dock->show();
    resizeDocks({projectDock_, propertyDock_}, {250, 330}, Qt::Horizontal);
    if (outputCollapsed_) setOutputCollapsed(false);
    resizeDocks({outputDock_}, {140}, Qt::Vertical);
}

bool MainWindow::insertWidget(WidgetSlot slot, QWidget *widget)
{
    const int key = static_cast<int>(slot);
    if (QThread::currentThread() != thread() || !widget || widget == this || widget->isAncestorOf(this)
        || widget->thread() != thread() || !slotLayouts_.contains(key) || insertedWidgets_.value(key)) return false;
    // Do not accept a widget already owned by this window (including another slot).
    if (isAncestorOf(widget)) return false;
    auto *layout = slotLayouts_.value(key);
    while (auto *item = layout->takeAt(0)) {
        // Keep built-in displays alive for queued events, hidden behind the injected widget.
        item->widget()->hide();
        delete item;
    }
    layout->addWidget(widget);
    insertedWidgets_.insert(key, widget);
    return true;
}

void MainWindow::bindServices(const ServiceBindings &services)
{
    for (const auto &connection : serviceConnections_) disconnect(connection);
    serviceConnections_.clear();
    workflowView_ = {};
    workflow_ = services.workflow;
    tasks_ = services.tasks;
    projectEditingEnabled_ = services.projects && !services.projects->currentProject().projectId.isEmpty();
    if (draftReset_) draftReset_->setEnabled(projectEditingEnabled_);
    if (!currentPage_.isEmpty()) selectPage(currentPage_);
    registerContractMetaTypes();
    if (workflow_) {
        serviceConnections_.append(connect(workflow_, &IWorkflowService::workflowViewChanged, this,
            &MainWindow::displayWorkflow, Qt::QueuedConnection));
        // view()/requestRun() are synchronous contracts; do not call across threads.
        serviceConnections_.append(connect(workflow_, &QObject::destroyed, this, [this] {
            runAction_->setEnabled(false);
            suggestion_->setText(uiText(u"\u5de5\u4f5c\u6d41\u670d\u52a1\u5df2\u65ad\u5f00"));
        }));
    } else {
        workflowTree_->clear();
        suggestion_->setText(uiText(u"\u7b49\u5f85\u5de5\u4f5c\u6d41\u670d\u52a1"));
    }
    if (tasks_) serviceConnections_.append(connect(tasks_, &ITaskService::taskEventOccurred, this,
        &MainWindow::displayTaskEvent, Qt::QueuedConnection));
    if (runAction_) runAction_->setEnabled(false);
    if (workflow_ && workflow_->thread() == thread()) displayWorkflow(workflow_->view(runRequest_.scope));
}

void MainWindow::setRunContext(const RunRequest &request)
{
    workflowView_ = {};
    runRequest_ = request;
    hasRunContext_ = !request.inputSnapshot.snapshotId.isEmpty();
    if (workflow_ && workflow_->thread() == thread()) displayWorkflow(workflow_->view(request.scope));
}

void MainWindow::displayWorkflow(const WorkflowView &view)
{
    if (hasRunContext_ && (view.scope.projectId != runRequest_.scope.projectId || view.scope.caseId != runRequest_.scope.caseId)) return;
    if (view.scope.projectId == workflowView_.scope.projectId && view.scope.caseId == workflowView_.scope.caseId
        && view.viewRevision < workflowView_.viewRevision) return;
    workflowView_ = view;
    workflowTree_->clear();
    suggestion_->setText(view.nextSuggestion);
    if (runAction_) runAction_->setEnabled(false);
    for (const auto &step : view.steps) {
        auto *item = new QTreeWidgetItem(workflowTree_, {step.summary, stepStatus(step.state)});
        item->setData(0, Qt::UserRole, step.targetPageId);
    }
    for (const auto &action : view.actions) {
        auto *item = new QTreeWidgetItem(workflowTree_, {action.actionId, action.enabled ? uiText(u"\u53ef\u7528") : uiText(u"\u4e0d\u53ef\u7528")});
        item->setToolTip(0, action.reason);
        item->setData(0, Qt::UserRole, action.targetPageId);
        item->setData(0, Qt::UserRole + 1, action.objectId);
        item->setData(0, Qt::UserRole + 2, action.fieldKey);
        if (action.actionId == "run" && runAction_) {
            runAction_->setEnabled(action.enabled && hasRunContext_ && workflow_ && workflow_->thread() == thread());
            runAction_->setToolTip(action.reason.isEmpty() ? uiText(u"\u5411\u5de5\u4f5c\u6d41\u670d\u52a1\u8bf7\u6c42\u8fd0\u884c") : action.reason);
        }
    }
    for (const auto &issue : view.issues) {
        auto *item = new QTreeWidgetItem(workflowTree_, {issue.message, uiText(u"\u6821\u9a8c\u63d0\u793a")});
        item->setData(0, Qt::UserRole, issue.targetPageId);
        item->setData(0, Qt::UserRole + 1, issue.objectId);
        item->setData(0, Qt::UserRole + 2, issue.fieldKey);
    }
}

void MainWindow::submitRun()
{
    if (!workflow_ || workflow_->thread() != thread() || !hasRunContext_) return;
    const auto result = workflow_->requestRun(runRequest_);
    appendMessage(result.allowed ? uiText(u"\u8fd0\u884c\u8bf7\u6c42\u5df2\u63d0\u4ea4\uff0c\u7b49\u5f85\u4efb\u52a1\u4e8b\u4ef6") : uiText(u"\u5de5\u4f5c\u6d41\u62d2\u7edd\u8fd0\u884c\u8bf7\u6c42"));
    for (const auto &issue : result.issues)
        appendLog(QStringLiteral("ERROR"), QStringLiteral("Validation"), issue.message, QStringLiteral("current-project"));
    if (!result.allowed) showOutput(3);
}

void MainWindow::displayTaskEvent(const TaskEvent &event)
{
    QTreeWidgetItem *row = nullptr;
    for (int i = 0; i < taskTree_->topLevelItemCount(); ++i) {
        if (taskTree_->topLevelItem(i)->text(0) == event.taskId) { row = taskTree_->topLevelItem(i); break; }
    }
    if (!row) {
        if (taskTree_->topLevelItemCount() >= 500) delete taskTree_->takeTopLevelItem(0);
        row = new QTreeWidgetItem(taskTree_, {event.taskId});
    }
    row->setText(1, taskStatus(event.status));
    row->setText(2, event.message);
    row->setText(3, event.timestamp.toLocalTime().toString("HH:mm:ss"));
    QTreeWidgetItem *navigationTask = nullptr;
    QTreeWidgetItem *singleRuns = nullptr;
    for (QTreeWidgetItemIterator it(monitorTree_); *it; ++it) {
        const QString objectId = (*it)->data(0, ObjectIdRole).toString();
        if (objectId == event.taskId) navigationTask = *it;
        if (objectId == QStringLiteral("single-runs")) singleRuns = *it;
    }
    if (!navigationTask && singleRuns && !event.taskId.isEmpty()) {
        navigationTask = addNavigationItem(singleRuns, event.taskId + uiText(u"\uff08Mock \u4e8b\u4ef6\uff09"),
                                           QStringLiteral("task"), event.taskId, QStringLiteral("task"));
    }
    if (navigationTask) {
        setTaskItemStatus(navigationTask, event.status);
        navigationTask->setToolTip(0, uiText(u"\u72b6\u6001\uff1a%1\n%2").arg(taskStatus(event.status), event.message));
    }
    monitorStatus_->setText(event.taskId + uiText(u" \u00b7 ") + taskStatus(event.status));
    if (event.progress.totalSteps > 0) {
        const int progressValue = qBound(0, static_cast<int>(1000.0 * event.progress.currentStep / event.progress.totalSteps), 1000);
        progress_->setValue(progressValue);
        progress_->setFormat(QStringLiteral("%1%").arg(progressValue / 10.0, 0, 'f', 1));
        monitorDetails_->setText(uiText(u"\u8fed\u4ee3\uff1a%1 / %2    \u6b8b\u5dee\uff1a%3").arg(event.progress.currentStep).arg(event.progress.totalSteps).arg(event.progress.residual));
    } else {
        progress_->setValue(0);
        progress_->setFormat(uiText(u"\u6682\u65e0\u8fdb\u5ea6\u6570\u636e"));
        monitorDetails_->setText(uiText(u"\u6682\u65e0\u8fed\u4ee3 / \u6b8b\u5dee\u6570\u636e\uff1bMock \u6d88\u606f\u4e0d\u4ee3\u8868\u771f\u5b9e\u8ba1\u7b97"));
    }
    const QString level = event.type == TaskEventType::Failed ? QStringLiteral("ERROR")
        : event.type == TaskEventType::Warning ? QStringLiteral("WARN") : QStringLiteral("INFO");
    appendLog(level, QStringLiteral("Task"),
              (event.errorCode.isEmpty() ? QString() : event.errorCode + QLatin1Char(' ')) + event.message,
              event.taskId);
}

void MainWindow::appendMessage(const QString &message)
{
    appendLog(QStringLiteral("INFO"), QStringLiteral("UI"), message,
              outputScope_.isEmpty() ? QStringLiteral("current-project") : outputScope_);
}

void MainWindow::appendLog(const QString &level, const QString &source, const QString &message,
                           const QString &scope)
{
    if (!outputLogTable_) return;
    const int row = outputLogTable_->rowCount();
    outputLogTable_->insertRow(row);
    const QStringList values = {QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), level, source, message};
    for (int column = 0; column < values.size(); ++column) {
        auto *item = new QTableWidgetItem(values[column]);
        item->setData(Qt::UserRole, level);
        item->setData(Qt::UserRole + 1, source);
        item->setData(Qt::UserRole + 2, scope);
        outputLogTable_->setItem(row, column, item);
    }
    QColor levelColor;
    if (level == QStringLiteral("WARN")) levelColor = QColor(QStringLiteral("#d88900"));
    else if (level == QStringLiteral("ERROR")) levelColor = QColor(QStringLiteral("#d04444"));
    else if (level == QStringLiteral("DEBUG")) levelColor = QColor(QStringLiteral("#7f8c8d"));
    if (levelColor.isValid()) outputLogTable_->item(row, 1)->setForeground(levelColor);
    applyLogFilters();
    if (logAutoScroll_ && logAutoScroll_->isChecked() && !outputLogTable_->isRowHidden(row))
        outputLogTable_->scrollToBottom();
}

void MainWindow::applyLogFilters()
{
    if (!outputLogTable_ || !logLevelFilter_ || !logModuleFilter_ || !logSearch_) return;
    const QString level = logLevelFilter_->currentData().toString();
    const QString source = logModuleFilter_->currentData().toString();
    const QString search = logSearch_->text().trimmed();
    for (int row = 0; row < outputLogTable_->rowCount(); ++row) {
        auto *first = outputLogTable_->item(row, 0);
        if (!first) continue;
        const bool levelMatches = level == QStringLiteral("ALL") || first->data(Qt::UserRole).toString() == level;
        const bool sourceMatches = source == QStringLiteral("ALL") || first->data(Qt::UserRole + 1).toString() == source;
        const bool scopeMatches = outputScope_.isEmpty() || first->data(Qt::UserRole + 2).toString() == outputScope_;
        bool searchMatches = search.isEmpty();
        for (int column = 0; column < outputLogTable_->columnCount() && !searchMatches; ++column) {
            auto *item = outputLogTable_->item(row, column);
            searchMatches = item && item->text().contains(search, Qt::CaseInsensitive);
        }
        outputLogTable_->setRowHidden(row, !(levelMatches && sourceMatches && scopeMatches && searchMatches));
    }
}

void MainWindow::setOutputScope(const QString &scope)
{
    outputScope_ = scope;
    const QString label = scope.isEmpty() ? uiText(u"\u5168\u90e8\u65e5\u5fd7")
        : scope == QStringLiteral("current-project") ? uiText(u"\u5f53\u524d\u5de5\u7a0b") : scope;
    if (outputTitle_) outputTitle_->setText(uiText(u"\u8f93\u51fa \u00b7 %1").arg(label));
    if (outputScopeReset_) outputScopeReset_->setVisible(!scope.isEmpty());
    applyLogFilters();
}

void MainWindow::setOutputCollapsed(bool collapsed)
{
    if (!outputDock_ || !outputBody_ || !outputCollapse_) return;
    outputCollapsed_ = collapsed;
    if (collapsed) {
        expandedOutputHeight_ = qMax(120, outputDock_->height());
        outputBody_->hide();
        outputCollapse_->setText(uiText(u"\u5c55\u5f00"));
        const int compactHeight = outputCollapse_->parentWidget()->sizeHint().height() + 4;
        outputDock_->setMinimumHeight(compactHeight);
        outputDock_->setMaximumHeight(compactHeight);
        resizeDocks({outputDock_}, {compactHeight}, Qt::Vertical);
    } else {
        outputDock_->setMinimumHeight(60);
        outputDock_->setMaximumHeight(QWIDGETSIZE_MAX);
        outputBody_->show();
        outputCollapse_->setText(uiText(u"\u6536\u8d77"));
        resizeDocks({outputDock_}, {qMax(120, expandedOutputHeight_)}, Qt::Vertical);
    }
}

void MainWindow::showOutput(int index)
{
    outputDock_->show();
    outputDock_->raise();
    if (outputCollapsed_) setOutputCollapsed(false);
    if (index == 3) logLevelFilter_->setCurrentIndex(logLevelFilter_->findData(QStringLiteral("ERROR")));
    else if (index == 0) logLevelFilter_->setCurrentIndex(logLevelFilter_->findData(QStringLiteral("ALL")));
}

} // namespace aerosim
