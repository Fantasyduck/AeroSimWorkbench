#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/GoverningEquationsPanel.h"
#include "aerosim/ui/BoundaryConditionsPanel.h"
#include "aerosim/ui/MeshMotionPanel.h"
#include "aerosim/ui/ReferenceValuesPanel.h"
#include "aerosim/ui/CalculationControlPanel.h"
#include "aerosim/ui/SolutionMethodPanel.h"
#include "aerosim/ui/OutputControlPanel.h"
#include "aerosim/ui/InitializationPanel.h"
#include "aerosim/ui/MeshManagementPage.h"
#include "aerosim/ui/monitor/ComputeMonitorPage.h"
#include "aerosim/ui/intelligent/ResultDataTablePage.h"
#include "aerosim/ui/intelligent/ResultSummaryTablePage.h"
#include "aerosim/ui/TrajectorySettingsPage.h"

#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>
#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QPalette>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFontDatabase>
#include <QFrame>
#include <QGuiApplication>
#include <QGroupBox>
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
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSpinBox>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTextCursor>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QtAlgorithms>
#include <QUuid>
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
// User-facing projection of the V2 workflow dimensions. It preserves the
// historical legacy labels: a dependency blocker dominates, then a stale or
// invalid validation, then a completed-and-valid step; non-applicable steps and
// every other combination stay "pending".
QString stepStatus(const WorkflowStepViewV2 &step)
{
    if (!step.blockers.isEmpty()) return uiText(u"\u88ab\u963b\u585e");
    if (!step.applicable) return uiText(u"\u5f85\u914d\u7f6e");
    if (step.validation == ValidationState::Stale
        || step.validation == ValidationState::Invalid)
        return uiText(u"\u5f85\u6821\u9a8c");
    if (step.completion == CompletionState::Completed
        && step.validation == ValidationState::Valid)
        return uiText(u"\u5df2\u5b8c\u6210");
    return uiText(u"\u5f85\u914d\u7f6e");
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
    status_ = new QLabel(uiText(u"\u5c31\u7eea  |  \u5de5\u4f5c\u533a\uff1a\u672a\u6253\u5f00  |  \u7f51\u683c\uff1a\u672a\u8f7d\u5165  |  \u6c42\u89e3\u5668\uff1a\u672a\u63a5\u5165  |  MPI\uff1a\u2014"), this);
    statusBar()->addWidget(status_, 1);
    statusBar()->addPermanentWidget(new QLabel(uiText(u"UI \u6846\u67b6 \u00b7 Mock \u670d\u52a1"), this));
    QTimer::singleShot(0, this, [this] {
        QSettings settings(QStringLiteral("AeroSimWorkbench"), QStringLiteral("Appearance"));
        applyWindowTheme(settings.value(QStringLiteral("theme"), QStringLiteral("office-blue")).toString());
    });
    restoreLayout();
    // Apply the integrated mesh page's compact dock arrangement only after
    // all docks have been created and positioned.
    showWorkspacePage(QStringLiteral("viewport"));
    appendMessage(uiText(u"\u4e3b\u754c\u9762\u5df2\u5c31\u7eea\u3002\u5de5\u4f5c\u533a\u6811\u4e0e\u53c2\u6570\u4e3a\u754c\u9762\u793a\u4f8b\uff1b\u5c1a\u672a\u63a5\u5165\u5de5\u4f5c\u533a\u3001\u914d\u7f6e\u3001VTK \u548c\u771f\u5b9e\u6c42\u89e3\u670d\u52a1\u3002"));
}

void MainWindow::createRibbon()
{
    ribbonBar()->setRibbonStyle(SARibbonBar::RibbonStyleLooseThreeRow);
    if (auto *startupButton = ribbonBar()->applicationButton()) {
        startupButton->setObjectName(QStringLiteral("returnToStartupButton"));
        startupButton->setText(uiText(u"\u8fd4\u56de\u6b22\u8fce\u9875"));
        startupButton->setIcon(QIcon(QStringLiteral(":/aerosim/ribbon/help.svg")));
        startupButton->setToolTip(uiText(u"\u9690\u85cf\u5f53\u524d\u5de5\u4f5c\u53f0\u5e76\u8fd4\u56de\u542f\u52a8\u6b22\u8fce\u9875"));
        connect(startupButton, &QAbstractButton::clicked, this, &MainWindow::showStartupRequested);
    }
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
            } else if (route.startsWith("parameter:")) {
                const QString pageId = route.mid(10);
                connect(action, &QAction::triggered, this, [this, pageId, action] {
                    showParameterDialog(pageId, action->text());
                });
            } else if (route.startsWith("output:")) {
                const int index = route.mid(7).toInt();
                connect(action, &QAction::triggered, this, [this, index] { showOutput(index); });
            } else if (route.startsWith("workspace:")) {
                const QString workspace = route.mid(10);
                connect(action, &QAction::triggered, this, [this, workspace] {
                    if (workspace == QStringLiteral("result-data-table")
                        || workspace == QStringLiteral("result-summary-table"))
                        selectNavigationNode(projectTree_, workspace);
                    showWorkspacePage(workspace);
                });
            } else if (route == "postprocess") {
                action->setObjectName(QStringLiteral("openPostProcessAction"));
                connect(action, &QAction::triggered, this, &MainWindow::openPostProcessRequested);
            } else if (route == "run") {
                runAction_ = action;
                action->setObjectName(QStringLiteral("runAction"));
                action->setEnabled(false);
                action->setToolTip(uiText(u"\u4e00\u6b21\u8ba1\u7b97\u5bf9\u5e94\u4e00\u4e2a\u72b6\u6001\uff1b\u7b49\u5f85\u5de5\u4f5c\u6d41\u63d0\u4f9b\u6709\u6548\u8f93\u5165\u4e0e\u8fd0\u884c\u6743\u9650"));
                connect(action, &QAction::triggered, this, &MainWindow::submitRun);
            } else if (route == "layout-default") {
                connect(action, &QAction::triggered, this, &MainWindow::restoreLayout);
            } else if (route == "layout-save") {
                action->setObjectName(QStringLiteral("saveUserLayoutAction"));
                connect(action, &QAction::triggered, this, &MainWindow::saveUserLayout);
            } else if (route == "layout-restore") {
                action->setObjectName(QStringLiteral("restoreUserLayoutAction"));
                QSettings settings(QStringLiteral("AeroSimWorkbench"), QStringLiteral("UiLayout"));
                action->setEnabled(settings.value(QStringLiteral("UserLayout/version")).toInt() == 1);
                restoreUserLayoutAction_ = action;
                connect(action, &QAction::triggered, this, &MainWindow::restoreUserLayout);
            } else if (route == "help-usage") {
                action->setObjectName(QStringLiteral("usageHelpAction"));
                connect(action, &QAction::triggered, this, [this] {
                    QMessageBox::information(this, uiText(u"\u4f7f\u7528\u5e2e\u52a9"),
                        uiText(u"\u5e2e\u52a9\u6587\u6863\u5c1a\u672a\u63a5\u5165\u3002\n\n\u53ef\u901a\u8fc7\u9876\u90e8 Ribbon \u4e0e\u5de6\u4fa7\u7edf\u4e00\u5de5\u4f5c\u533a\u6811\u8fdb\u884c\u529f\u80fd\u5bfc\u822a\u3002"));
                });
            } else if (route == "about") {
                action->setObjectName(QStringLiteral("aboutSoftwareAction"));
                connect(action, &QAction::triggered, this, [this] {
                    QMessageBox::about(this, uiText(u"\u5173\u4e8e AeroSimWorkbench"),
                        uiText(u"AeroSimWorkbench\n\u4eff\u771f\u5de5\u4f5c\u53f0 UI \u6846\u67b6\nQt 5.12.12 / SARibbon / C++14\n\n\u5f53\u524d\u4e1a\u52a1\u6570\u636e\u4e0e\u90e8\u5206\u754c\u9762\u4e3a Mock \u6f14\u793a\u3002"));
                });
            } else if (route == "startup") {
                action->setObjectName(QStringLiteral("showStartupAction"));
                connect(action, &QAction::triggered, this, &MainWindow::showStartupRequested);
            } else if (route.startsWith("dock:")) {
                auto *dock = route == "dock:project" ? projectDock_
                    : route == "dock:property" ? propertyDock_ : outputDock_;
                connect(action, &QAction::triggered, this, [this, dock] {
                    if (dock == propertyDock_ && meshWorkspaceActive_) {
                        statusBar()->showMessage(
                            uiText(u"\u7f51\u683c\u663e\u793a\u5c5e\u6027\u5df2\u6574\u5408\u5230\u5f53\u524d\u7f51\u683c\u7ba1\u7406\u9875\u9762"), 4000);
                        return;
                    }
                    dock->show();
                });
            }
        }
        return panel;
    };
    auto *preprocess = ribbonBar()->addCategoryPage(uiText(u"\u524d\u5904\u7406"));
    preprocess->setObjectName(QStringLiteral("preprocessCategory"));
    auto *workspacePanel = addGroup(preprocess, "workspaceGroup", uiText(u"\u5de5\u4f5c\u533a"), {
        {"open-project", uiText(u"\u6253\u5f00\u5de5\u4f5c\u533a"), false, ""},
        {"save-project", uiText(u"\u4fdd\u5b58\u5de5\u4f5c\u533a"), false, ""},
        {"help", uiText(u"\u8fd4\u56de\u6b22\u8fce\u9875"), false, "startup"}});
    auto *newProjectMenu = new QMenu(uiText(u"\u65b0\u5efa\u5de5\u4f5c\u533a"), this);
    newProjectMenu->setObjectName(QStringLiteral("newProjectMenu"));
    newProjectMenu->setIcon(QIcon(QStringLiteral(":/aerosim/ribbon/new-project.svg")));
    const struct { const char *id; QString label; const char *icon; } projectEntries[] = {
        {"blankProjectMenuAction", uiText(u"\u7a7a\u767d\u5de5\u4f5c\u533a"), "new-project"},
        {"templateProjectMenuAction", uiText(u"\u4ece\u6a21\u677f\u65b0\u5efa"), "template"},
        {"importProjectMenuAction", uiText(u"\u5bfc\u5165\u5de5\u4f5c\u533a\u914d\u7f6e"), "import-config"}};
    for (const auto &entry : projectEntries) {
        auto *action = newProjectMenu->addAction(
            QIcon(QStringLiteral(":/aerosim/ribbon/") + QString::fromLatin1(entry.icon) + QStringLiteral(".svg")),
            entry.label);
        action->setObjectName(QString::fromLatin1(entry.id));
        action->setEnabled(false);
        action->setToolTip(uiText(u"\u7b49\u5f85\u5de5\u4f5c\u533a\u670d\u52a1\u63a5\u5165"));
    }
    newProjectMenu->menuAction()->setObjectName(QStringLiteral("newProjectMenuAction"));
    newProjectMenu->menuAction()->setProperty("ribbonGroup", QStringLiteral("workspaceGroup"));
    newProjectMenu->menuAction()->setToolTip(uiText(u"\u9009\u62e9\u65b0\u5efa\u5de5\u4f5c\u533a\u7684\u65b9\u5f0f"));
    workspacePanel->addLargeMenu(newProjectMenu);
    workspacePanel->moveAction(workspacePanel->actionIndex(newProjectMenu->menuAction()), 0);
    // Preprocess keeps a single unified entry per page. The former mesh file /
    // transform / boundary / info entries and the trajectory file / data /
    // state-extraction entries are gone; their in-page content stays inside the
    // two consolidated pages instead of separate navigation targets.
    addGroup(preprocess, "meshManagementGroup", uiText(u"\u7f51\u683c\u7ba1\u7406"), {
        {"mesh", uiText(u"\u7f51\u683c\u7ba1\u7406"), true, "workspace:viewport"}});
    addGroup(preprocess, "trajectoryGroup", uiText(u"\u5f39\u9053\u8bbe\u7f6e"), {
        {"curve", uiText(u"\u5f39\u9053\u8bbe\u7f6e"), true, "workspace:trajectory-settings"}});
    auto *viewPanel = addGroup(preprocess, "viewGroup", uiText(u"\u89c6\u56fe"), {
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
    auto *parameters = ribbonBar()->addCategoryPage(uiText(u"\u53c2\u6570\u8bbe\u7f6e"));
    parameters->setObjectName(QStringLiteral("parametersCategory"));
    addGroup(parameters, "equationGroup", uiText(u"\u7269\u7406\u4e0e\u8fb9\u754c"), {
        {"physics", uiText(u"\u63a7\u5236\u65b9\u7a0b"), true, "parameter:control-equations"},
        {"boundary", uiText(u"\u8fb9\u754c\u6761\u4ef6"), true, "parameter:boundary-conditions"},
        {"transform", uiText(u"\u7f51\u683c\u8fd0\u52a8"), true, "parameter:mesh-motion"}});
    addGroup(parameters, "solverSettingsGroup", uiText(u"\u6c42\u89e3\u914d\u7f6e"), {
        {"settings", uiText(u"\u6c42\u89e3\u65b9\u6cd5"), true, "parameter:solution-method"},
        {"atmosphere", uiText(u"\u53c2\u8003\u91cf"), true, "parameter:reference-values"},
        {"output", uiText(u"\u8f93\u51fa\u63a7\u5236"), true, "parameter:output-control"},
        {"initialize", uiText(u"\u521d\u59cb\u5316"), true, "parameter:initialization"}});

    auto *calculation = ribbonBar()->addCategoryPage(uiText(u"\u8ba1\u7b97"));
    calculation->setObjectName(QStringLiteral("calculationCategory"));
    addGroup(calculation, "conditionGroup", uiText(u"\u5de5\u51b5\u4e0e\u8ba1\u5212"), {
        {"state", uiText(u"\u5de5\u51b5\u7ba1\u7406"), true, "workspace:states"},
        {"batch", uiText(u"\u6279\u5904\u7406\u8ba1\u5212"), true, "workspace:batch-plan"}});
    addGroup(calculation, "calculationGroup", uiText(u"\u5f00\u59cb\u8ba1\u7b97"), {
        {"run", uiText(u"\u5355\u6b21\u8ba1\u7b97"), true, "run"},
        {"queue", uiText(u"\u6279\u91cf\u8ba1\u7b97"), true, "workspace:batch-calculation"}});
    addGroup(calculation, "monitorGroup", uiText(u"\u8ba1\u7b97\u76d1\u63a7"), {
        {"monitor", uiText(u"\u4efb\u52a1\u76d1\u63a7"), true, "workspace:task-monitor"},
        {"residual", uiText(u"\u6536\u655b\u66f2\u7ebf"), false, "workspace:task-monitor"},
        {"log", uiText(u"\u8f93\u51fa"), false, "output:0"}});

    auto *data = ribbonBar()->addCategoryPage(uiText(u"\u667a\u80fd\u6570\u636e\u5904\u7406"));
    data->setObjectName(QStringLiteral("intelligentDataCategory"));
    auto *resultPanel = addGroup(data, "intelligentDataGroup", uiText(u"\u529f\u80fd\u5165\u53e3"), {
        {"curve", uiText(u"结果数据表"), true, "workspace:result-data-table"},
        {"compare", uiText(u"结果汇总表"), true, "workspace:result-summary-table"}});
    resultPanel->actions().at(0)->setObjectName(QStringLiteral("resultDataTableAction"));
    resultPanel->actions().at(1)->setObjectName(QStringLiteral("resultSummaryTableAction"));

    auto *post = ribbonBar()->addCategoryPage(uiText(u"\u540e\u5904\u7406\u5206\u6790"));
    post->setObjectName(QStringLiteral("postprocessAnalysisCategory"));
    addGroup(post, "postprocessEntryGroup", uiText(u"\u529f\u80fd\u5165\u53e3"), {
        {"workbench", uiText(u"\u540e\u5904\u7406\u5206\u6790"), true, ""}});

    auto *help = ribbonBar()->addCategoryPage(uiText(u"\u5e2e\u52a9"));
    help->setObjectName(QStringLiteral("helpCategory"));
    auto *appearance = help->addPanel(uiText(u"\u5916\u89c2\u8bbe\u7f6e"));
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
    addGroup(help, "layoutToolsGroup", uiText(u"\u754c\u9762\u5e03\u5c40"), {
        {"export-config", uiText(u"\u4fdd\u5b58\u5f53\u524d\u5e03\u5c40"), true, "layout-save"},
        {"import-config", uiText(u"\u6062\u590d\u4e0a\u6b21\u5e03\u5c40"), true, "layout-restore"},
        {"layout", uiText(u"\u6062\u590d\u9ed8\u8ba4\u5e03\u5c40"), true, "layout-default"},
        {"navigation", uiText(u"\u5de5\u4f5c\u533a\u6811"), false, "dock:project"},
        {"property", uiText(u"\u5c5e\u6027\u9762\u677f"), false, "dock:property"},
        {"output", uiText(u"\u8f93\u51fa\u9762\u677f"), false, "dock:output"}});
    addGroup(help, "helpInfoGroup", uiText(u"\u5e2e\u52a9\u4fe1\u606f"), {
        {"help", uiText(u"\u4f7f\u7528\u5e2e\u52a9"), true, "help-usage"},
        {"about", uiText(u"\u5173\u4e8e\u8f6f\u4ef6"), true, "about"}});
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
    // Embedded parameter pages share the active theme, including the scroll
    // viewport. Child panels must not override these colors independently.
    setStyleSheet(styleSheet() + QStringLiteral(
        "QWidget[parameterEditor=\"true\"], QWidget[parameterSurface=\"true\"], QWidget[parameterPanel=\"true\"] { background: %1; color: %3; }"
        "QWidget[parameterEditor=\"true\"] QLabel { background: transparent; color: %3; font-weight: normal; }"
        "QWidget[parameterEditor=\"true\"] QLabel[parameterHint=\"true\"] { color: %6; }"
        "QWidget[parameterEditor=\"true\"] QLabel#parameterHeading { font-size: 18px; font-weight: 600; }"
        "QWidget[parameterEditor=\"true\"] QScrollArea { background: %1; border: 0; }"
        "QWidget[parameterEditor=\"true\"] QScrollBar:vertical { background: %1; width: 10px; margin: 0; }"
        "QWidget[parameterEditor=\"true\"] QScrollBar::handle:vertical { background: %4; min-height: 28px; border-radius: 4px; }"
        "QWidget[parameterEditor=\"true\"] QScrollBar::add-line:vertical, QWidget[parameterEditor=\"true\"] QScrollBar::sub-line:vertical { height: 0; }"
        "QWidget[parameterEditor=\"true\"] QScrollBar::add-page:vertical, QWidget[parameterEditor=\"true\"] QScrollBar::sub-page:vertical { background: %1; }"
        "QWidget[parameterEditor=\"true\"] QGroupBox { background: %2; color: %3; border: 1px solid %4; border-radius: 6px; margin-top: 12px; padding: 20px 16px 16px; font-weight: 600; }"
        "QWidget[parameterEditor=\"true\"] QGroupBox::title { subcontrol-origin: margin; left: 16px; padding: 0 5px; color: %3; }"
        "QWidget[parameterEditor=\"true\"] QRadioButton, QWidget[parameterEditor=\"true\"] QCheckBox { background: transparent; color: %3; spacing: 8px; min-height: 26px; font-weight: normal; }"
        "QWidget[parameterEditor=\"true\"] QLineEdit, QWidget[parameterEditor=\"true\"] QComboBox, QWidget[parameterEditor=\"true\"] QAbstractSpinBox { background: %2; color: %3; border: 1px solid %4; border-radius: 4px; min-height: 28px; padding: 4px 8px; font-weight: normal; selection-background-color: %5; }"
        "QWidget[parameterEditor=\"true\"] QLineEdit:focus, QWidget[parameterEditor=\"true\"] QComboBox:focus, QWidget[parameterEditor=\"true\"] QAbstractSpinBox:focus { border-color: #638fc6; }"
        "QWidget[parameterEditor=\"true\"] QLineEdit:disabled, QWidget[parameterEditor=\"true\"] QComboBox:disabled, QWidget[parameterEditor=\"true\"] QAbstractSpinBox:disabled { background: %1; color: %6; }"
        "QWidget[parameterEditor=\"true\"] QTreeWidget, QWidget[parameterEditor=\"true\"] QTableWidget { background: %2; alternate-background-color: %1; color: %3; border: 1px solid %4; border-radius: 5px; }"
        "QWidget[parameterEditor=\"true\"] QTreeWidget::item { padding: 7px 8px; }"
        "QWidget[parameterEditor=\"true\"] QHeaderView::section { background: %1; color: %3; padding: 8px; border: 0; border-bottom: 1px solid %4; }"
        "QWidget[parameterEditor=\"true\"] QPushButton { border-radius: 4px; }"
        "QWidget[parameterEditor=\"true\"] QPushButton#applyBoundary { background: #2563b6; border-color: #2563b6; color: white; padding: 7px 24px; }"
        "QWidget[parameterEditor=\"true\"] QPushButton#applyBoundary:disabled { background: %1; color: %6; border-color: %4; }")
        .arg(bg, base, text, border, selected, QStringLiteral("#61748a")));
    setStyleSheet(styleSheet() + QStringLiteral(
        "QWidget#parameterEditor_control-equations QGroupBox { padding: 16px 18px 14px; }"
        "QWidget#parameterEditor_reference-values QGroupBox { padding: 16px 18px 14px; }"
        "QWidget#parameterEditor_calculation-control QGroupBox { padding: 16px 18px 14px; }"
        "QWidget#parameterEditor_solution-method QGroupBox { padding: 16px 18px 14px; }"
        "QWidget#parameterEditor_solution-method QAbstractSpinBox, QWidget#parameterEditor_solution-method QComboBox { min-height: 22px; max-height: 22px; padding: 2px 6px; }"
        "QWidget#parameterEditor_output-control QGroupBox { padding: 16px 18px 14px; }"
        "QWidget#parameterEditor_output-control QAbstractSpinBox { min-height: 22px; max-height: 22px; padding: 2px 6px; }"
        "QWidget#parameterEditor_initialization QGroupBox { padding: 16px 18px 14px; }"
        "QWidget#parameterEditor_initialization QAbstractSpinBox, QWidget#parameterEditor_initialization QComboBox { min-height: 22px; max-height: 22px; padding: 2px 6px; }"
        "QWidget#parameterEditor_boundary-conditions QGroupBox { padding: 16px 18px 14px; }"
        "QWidget#parameterEditor_boundary-conditions QAbstractSpinBox, QWidget#parameterEditor_boundary-conditions QComboBox, QWidget#parameterEditor_boundary-conditions QLineEdit { min-height: 22px; max-height: 22px; padding: 2px 6px; }"
        "QWidget#parameterEditor_calculation-control QAbstractSpinBox, QWidget#parameterEditor_calculation-control QComboBox { min-height: 22px; max-height: 22px; padding: 2px 6px; }"
        "QWidget#parameterEditor_reference-values QAbstractSpinBox, QWidget#parameterEditor_reference-values QComboBox { min-height: 22px; max-height: 22px; padding: 2px 6px; }"
        "QWidget#parameterEditor_control-equations QAbstractSpinBox { min-height: 22px; max-height: 22px; padding: 2px 6px; }"
        "QWidget#parameterEditor_control-equations QRadioButton, QWidget#parameterEditor_control-equations QCheckBox { min-height: 28px; }"));
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
    auto *viewport = createSlot(WidgetSlot::Viewport, uiText(u"\u4e09\u7ef4\u5de5\u4f5c\u533a\n\nVTK 8.2 \u89c6\u53e3\u63a5\u5165\u4f4d\u7f6e\n\u5bfc\u5165\u4e0e\u6e32\u67d3\u6a21\u5757\u5c1a\u672a\u63a5\u5165"));
    auto *meshPage = new MeshManagementPage(viewport, workspaceStack_);
    viewportPage_ = meshPage;
    workspaceStack_->addWidget(meshPage);
    // The replicated mesh workspace offers a 后处理 tab that opens the same
    // standalone post-process workbench as the Ribbon entry.
    connect(meshPage, &MeshManagementPage::postProcessRequested,
            this, &MainWindow::openPostProcessRequested);
    // Unified trajectory settings page. The former trajectory leaf pages are
    // consolidated here; the page keeps all of its state in the window and no
    // longer depends on the deleted navigation entries.
    trajectorySettingsPage_ = new TrajectorySettingsPage(workspaceStack_);
    workspaceStack_->addWidget(trajectorySettingsPage_);

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
    parameterWorkspacePage_ = createWorkspacePage(
        QStringLiteral("parameterWorkspacePage"), uiText(u"\u53c2\u6570\u8bbe\u7f6e"),
        uiText(u"\u4ece\u5de6\u4fa7\u7edf\u4e00\u5de5\u4f5c\u533a\u6811\u9009\u62e9\u63a7\u5236\u65b9\u7a0b\u3001\u8fb9\u754c\u6761\u4ef6\u3001\u7f51\u683c\u8fd0\u52a8\u3001\u6c42\u89e3\u65b9\u6cd5\u3001\u53c2\u8003\u91cf\u3001\u8f93\u51fa\u63a7\u5236\u6216\u521d\u59cb\u5316\u3002\u5f53\u524d\u4ec5\u63d0\u4f9b\u524d\u7aef Mock \u8349\u7a3f\u4e0e\u5df2\u6709\u914d\u7f6e\u5f39\u7a97\uff0c\u4e0d\u6267\u884c\u4e1a\u52a1\u6821\u9a8c\u6216\u6301\u4e45\u5316\u3002"));
    parameterWorkspaceTitle_ = parameterWorkspacePage_->findChild<QLabel *>(
        QStringLiteral("workspacePageTitle"), Qt::FindDirectChildrenOnly);
    if (parameterWorkspaceTitle_) parameterWorkspaceTitle_->setObjectName(QStringLiteral("parameterWorkspaceTitle"));
    auto *parameterCategories = new QLabel(
        uiText(u"\u53ef\u7528\u7c7b\u522b\n\n\u2022 \u63a7\u5236\u65b9\u7a0b    \u2022 \u8fb9\u754c\u6761\u4ef6    \u2022 \u7f51\u683c\u8fd0\u52a8\n\u2022 \u6c42\u89e3\u65b9\u6cd5    \u2022 \u53c2\u8003\u91cf        \u2022 \u8f93\u51fa\u63a7\u5236    \u2022 \u521d\u59cb\u5316"),
        parameterWorkspacePage_);
    parameterCategories->setObjectName(QStringLiteral("parameterCategorySummary"));
    parameterCategories->setWordWrap(true);
    qobject_cast<QVBoxLayout *>(parameterWorkspacePage_->layout())->addWidget(parameterCategories);
    qobject_cast<QVBoxLayout *>(parameterWorkspacePage_->layout())->addStretch();

    calculationOverviewPage_ = createWorkspacePage(
        QStringLiteral("calculationOverviewPage"), uiText(u"\u8ba1\u7b97"),
        uiText(u"\u8ba1\u7b97\u6a21\u5757\u524d\u7aef\u5165\u53e3\u3002\u53ef\u5728\u5de6\u4fa7\u9009\u62e9\u5de5\u51b5\u7ba1\u7406\u3001\u6279\u5904\u7406\u8ba1\u5212\u3001\u5355\u6b21/\u6279\u91cf\u8ba1\u7b97\u6216\u8ba1\u7b97\u76d1\u63a7\u3002\u5f53\u524d\u6570\u636e\u4e0e\u8fd0\u884c\u72b6\u6001\u5747\u4e3a Mock \u754c\u9762\u793a\u4f8b\uff0c\u4e0d\u542f\u52a8\u771f\u5b9e\u6c42\u89e3\u5668\u3002"));
    auto *calculationOverviewLayout = qobject_cast<QVBoxLayout *>(calculationOverviewPage_->layout());
    auto *outputModeGroup = new QGroupBox(uiText(u"\u8fd0\u884c\u8f93\u51fa\u65b9\u5f0f"), calculationOverviewPage_);
    outputModeGroup->setObjectName(QStringLiteral("solverOutputModeGroup"));
    auto *outputModeLayout = new QFormLayout(outputModeGroup);
    auto *outputMode = new QComboBox(outputModeGroup);
    outputMode->setObjectName(QStringLiteral("solverOutputModeCombo"));
    outputMode->addItem(uiText(u"\u7a97\u53e3\u5185\u8f93\u51fa"), QStringLiteral("embedded"));
    outputMode->addItem(uiText(u"\u72ec\u7acb\u63a7\u5236\u53f0"), QStringLiteral("console"));
    auto *outputModeDescription = new QLabel(
        uiText(u"Mock \u914d\u7f6e\uff1a\u6c42\u89e3\u5668\u8f93\u51fa\u5c06\u5728\u8ba1\u7b97\u76d1\u63a7\u9875\u7684\u5f53\u524d\u4efb\u52a1\u65e5\u5fd7\u4e2d\u663e\u793a\u3002"), outputModeGroup);
    outputModeDescription->setObjectName(QStringLiteral("solverOutputModeDescription"));
    outputModeDescription->setWordWrap(true);
    outputModeLayout->addRow(uiText(u"\u8f93\u51fa\u4f4d\u7f6e"), outputMode);
    outputModeLayout->addRow(QString(), outputModeDescription);
    connect(outputMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [outputMode, outputModeDescription](int) {
        outputModeDescription->setText(outputMode->currentData().toString() == QStringLiteral("embedded")
            ? uiText(u"Mock \u914d\u7f6e\uff1a\u6c42\u89e3\u5668\u8f93\u51fa\u5c06\u5728\u8ba1\u7b97\u76d1\u63a7\u9875\u7684\u5f53\u524d\u4efb\u52a1\u65e5\u5fd7\u4e2d\u663e\u793a\u3002")
            : uiText(u"Mock \u914d\u7f6e\uff1a\u4fdd\u7559\u72ec\u7acb\u63a7\u5236\u53f0\u9009\u62e9\u72b6\u6001\uff0c\u672c\u671f\u4e0d\u521b\u5efa\u63a7\u5236\u53f0\u8fdb\u7a0b\u3002"));
    });
    calculationOverviewLayout->addWidget(outputModeGroup);
    calculationOverviewLayout->addStretch();

    stateListPage_ = createWorkspacePage(QStringLiteral("stateListPage"), uiText(u"\u5de5\u51b5\u7ba1\u7406"),
        uiText(u"\u5de5\u51b5\u4e0e\u6279\u5904\u7406\u8ba1\u5212\u5206\u5f00\u7ba1\u7406\u3002\u4ee5\u4e0b\u5185\u5bb9\u4ec5\u7528\u4e8e\u754c\u9762\u9a8c\u8bc1\uff0c\u672a\u4ece\u5de5\u4f5c\u533a\u670d\u52a1\u52a0\u8f7d\u3002"));
    auto *stateLayout = qobject_cast<QVBoxLayout *>(stateListPage_->layout());
    stateTable_ = new QTableWidget(0, 3, stateListPage_);
    stateTable_->setObjectName(QStringLiteral("stateTable"));
    stateTable_->setHorizontalHeaderLabels({uiText(u"\u5de5\u51b5"), uiText(u"\u53c2\u6570\u6458\u8981"), uiText(u"\u6821\u9a8c\u72b6\u6001")});
    stateTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    stateTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    stateTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    stateTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(stateTable_, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editState(); });
    stateLayout->addWidget(stateTable_, 1);
    auto *stateCommands = new QHBoxLayout;
    addStateButton_ = new QPushButton(uiText(u"+ \u65b0\u5efa\u5de5\u51b5"), stateListPage_);
    addStateButton_->setObjectName(QStringLiteral("addStateButton"));
    editStateButton_ = new QPushButton(uiText(u"\u7f16\u8f91\u53c2\u6570"), stateListPage_);
    editStateButton_->setObjectName(QStringLiteral("editStateButton"));
    removeStateButton_ = new QPushButton(uiText(u"- \u5220\u9664\u5de5\u51b5"), stateListPage_);
    removeStateButton_->setObjectName(QStringLiteral("removeStateButton"));
    connect(addStateButton_, &QPushButton::clicked, this, &MainWindow::addState);
    connect(editStateButton_, &QPushButton::clicked, this, &MainWindow::editState);
    connect(removeStateButton_, &QPushButton::clicked, this, &MainWindow::removeState);
    stateCommands->addWidget(addStateButton_);
    stateCommands->addWidget(editStateButton_);
    stateCommands->addWidget(removeStateButton_);
    stateCommands->addStretch();
    stateLayout->addLayout(stateCommands);
    refreshStateModel();

    batchPlanPage_ = createWorkspacePage(QStringLiteral("batchPlanPage"), uiText(u"\u6279\u5904\u7406\u8ba1\u5212"),
        uiText(u"\u6279\u5904\u7406\u8ba1\u5212\u662f\u672a\u6765\u7528\u4e8e\u4fdd\u5b58\u548c\u590d\u7528\u6279\u91cf\u8ba1\u7b97\u6a21\u677f\u7684\u5165\u53e3\u3002\u5f53\u524d\u7248\u672c\u8bf7\u4f7f\u7528\u201c\u6279\u91cf\u8ba1\u7b97\u201d\u914d\u7f6e\u5e76\u63d0\u4ea4\u4e00\u6b21\u6027\u6279\u5904\u7406\u4efb\u52a1\uff1b\u672c\u9875\u4ec5\u4f5c\u8bf4\u660e\u548c\u53ea\u8bfb\u5360\u4f4d\u3002"));
    auto *batchLayout = qobject_cast<QVBoxLayout *>(batchPlanPage_->layout());
    batchPlanTable_ = new QTableWidget(1, 3, batchPlanPage_);
    batchPlanTable_->setObjectName(QStringLiteral("batchPlanTable"));
    batchPlanTable_->setHorizontalHeaderLabels({uiText(u"\u5f53\u524d\u72b6\u6001"), uiText(u"\u4f5c\u7528"), uiText(u"\u64cd\u4f5c\u8bf4\u660e")});
    batchPlanTable_->setItem(0, 0, new QTableWidgetItem(uiText(u"\u672a\u521b\u5efa\u6b63\u5f0f BatchPlan")));
    batchPlanTable_->setItem(0, 1, new QTableWidgetItem(uiText(u"\u4fdd\u5b58\u53ef\u91cd\u7528\u7684\u6279\u91cf\u6a21\u677f")));
    batchPlanTable_->setItem(0, 2, new QTableWidgetItem(uiText(u"\u8bf7\u4ece\u201c\u6279\u91cf\u8ba1\u7b97\u201d\u914d\u7f6e\u548c\u63d0\u4ea4")));
    batchPlanTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    batchPlanTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    batchPlanTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    batchPlanTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    batchLayout->addWidget(batchPlanTable_, 1);
    auto *batchCommands = new QHBoxLayout;
    const QStringList batchCommandLabels = {uiText(u"\u6dfb\u52a0\u5de5\u51b5"), uiText(u"\u79fb\u9664"), uiText(u"\u4e0a\u79fb"), uiText(u"\u4e0b\u79fb"), uiText(u"\u67e5\u770b\u6821\u9a8c")};
    int batchCommandIndex = 0;
    for (const auto &label : batchCommandLabels) {
        auto *button = new QPushButton(label, batchPlanPage_);
        button->setEnabled(false);
        button->setToolTip(uiText(u"\u6279\u5904\u7406\u8ba1\u5212\u670d\u52a1\u5c1a\u672a\u63a5\u5165\uff1b\u8bf7\u4f7f\u7528\u201c\u6279\u91cf\u8ba1\u7b97\u201d"));
        batchCommands->addWidget(button);
        ++batchCommandIndex;
    }
    batchCommands->addStretch();
    batchLayout->addLayout(batchCommands);
    auto *newPlan = new QPushButton(uiText(u"\u65b0\u5efa\u6279\u5904\u7406\u8ba1\u5212"), batchPlanPage_);
    newPlan->setEnabled(false);
    newPlan->setToolTip(uiText(u"\u6b63\u5f0f BatchPlan \u670d\u52a1\u5c1a\u672a\u63a5\u5165\uff1b\u8bf7\u4f7f\u7528\u201c\u6279\u91cf\u8ba1\u7b97\u201d"));
    batchLayout->addWidget(newPlan, 0, Qt::AlignLeft);

    // These two pages are the user-facing entry points for creating execution
    // requests.  They deliberately use the existing RunRequest contract; the
    // real solver and persistent BatchPlan service can replace the Mock later.
    singleCalculationPage_ = createWorkspacePage(
        QStringLiteral("singleCalculationPage"), uiText(u"\u5355\u6b21\u8ba1\u7b97"),
        uiText(u"\u4e00\u7ec4\u5de5\u51b5\u53c2\u6570\u751f\u6210\u4e00\u4e2a\u6267\u884c\u8bf7\u6c42\u3002\u63d0\u4ea4\u540e\u5c06\u8fdb\u5165\u4efb\u52a1\u76d1\u63a7\uff1b\u5f53\u524d\u4f7f\u7528 Mock \u670d\u52a1\uff0c\u4e0d\u542f\u52a8\u771f\u5b9e\u6c42\u89e3\u5668\u3002"));
    auto *singleLayout = qobject_cast<QVBoxLayout *>(singleCalculationPage_->layout());
    auto *singleGroup = new QGroupBox(uiText(u"\u8ba1\u7b97\u5de5\u51b5\u4e0e\u53c2\u6570"), singleCalculationPage_);
    auto *singleForm = new QFormLayout(singleGroup);
    singleCaseCombo_ = new QComboBox(singleGroup);
    singleCaseCombo_->setObjectName(QStringLiteral("singleCaseCombo"));
    singleCaseCombo_->addItems(stateIds_);
    singleMachSpin_ = new QDoubleSpinBox(singleGroup);
    singleMachSpin_->setObjectName(QStringLiteral("singleMachSpin"));
    singleMachSpin_->setRange(0.01, 20.0); singleMachSpin_->setSingleStep(0.05); singleMachSpin_->setValue(0.80);
    singleAoASpin_ = new QDoubleSpinBox(singleGroup);
    singleAoASpin_->setObjectName(QStringLiteral("singleAoASpin"));
    singleAoASpin_->setRange(-90.0, 90.0); singleAoASpin_->setSingleStep(0.5); singleAoASpin_->setValue(2.0);
    singleTemperatureSpin_ = new QDoubleSpinBox(singleGroup);
    singleTemperatureSpin_->setObjectName(QStringLiteral("singleTemperatureSpin"));
    singleTemperatureSpin_->setRange(100.0, 2000.0); singleTemperatureSpin_->setSuffix(QStringLiteral(" K")); singleTemperatureSpin_->setValue(288.15);
    singleMpiSpin_ = new QSpinBox(singleGroup);
    singleMpiSpin_->setObjectName(QStringLiteral("singleMpiSpin"));
    singleMpiSpin_->setRange(1, 4096); singleMpiSpin_->setValue(4);
    singleOutputModeCombo_ = new QComboBox(singleGroup);
    singleOutputModeCombo_->setObjectName(QStringLiteral("singleOutputModeCombo"));
    singleOutputModeCombo_->addItem(uiText(u"\u63d0\u4ea4\u672c\u5730\u8ba1\u7b97"), QStringLiteral("local"));
    singleOutputModeCombo_->addItem(uiText(u"\u4ec5\u751f\u6210\u8f93\u5165\u6587\u4ef6"), QStringLiteral("input-only"));
    singleOutputModeCombo_->addItem(uiText(u"\u4ec5\u751f\u6210\u8ba1\u7b97\u538b\u7f29\u5305"), QStringLiteral("package-only"));
    singleForm->addRow(uiText(u"\u5de5\u51b5"), singleCaseCombo_);
    singleForm->addRow(uiText(u"\u9a6c\u8d6b\u6570"), singleMachSpin_);
    singleForm->addRow(uiText(u"\u653b\u89d2"), singleAoASpin_);
    singleForm->addRow(uiText(u"\u6e29\u5ea6"), singleTemperatureSpin_);
    singleForm->addRow(uiText(u"MPI \u8fdb\u7a0b\u6570"), singleMpiSpin_);
    singleForm->addRow(uiText(u"\u8f93\u51fa\u65b9\u5f0f"), singleOutputModeCombo_);
    // Keep the same condition data visible in the single-task form.  The
    // selected state is the source of truth; the dialog remains available for
    // a one-off run override.
    singleGroup->show();
    singleParametersSummary_ = new QLabel(singleCalculationPage_);
    singleParametersSummary_->setObjectName(QStringLiteral("singleParametersSummary"));
    singleParametersSummary_->setWordWrap(true);
    singleLayout->addWidget(singleParametersSummary_);
    auto *editSingleParameters = new QPushButton(uiText(u"\u7f16\u8f91\u672c\u6b21\u53c2\u6570"), singleCalculationPage_);
    editSingleParameters->setObjectName(QStringLiteral("editSingleParameters"));
    connect(editSingleParameters, &QPushButton::clicked, this, [this] {
        QString caseId = singleCaseCombo_->currentText();
        double mach = singleMachSpin_->value();
        double aoa = singleAoASpin_->value();
        double temperature = singleTemperatureSpin_->value();
        int mpi = singleMpiSpin_->value();
        if (!editCalculationParameters(caseId, mach, aoa, temperature, mpi)) return;
        singleCaseCombo_->setCurrentText(caseId);
        singleMachSpin_->setValue(mach);
        singleAoASpin_->setValue(aoa);
        singleTemperatureSpin_->setValue(temperature);
        singleMpiSpin_->setValue(mpi);
    singleParametersSummary_->setText(uiText(u"\u5f53\u524d\u5355\u6b21\u5de5\u51b5\uff1a%1\n\u9a6c\u8d6b\u6570 %2\uff0c\u653b\u89d2 %3\uff0c\u6e29\u5ea6 %4 K\uff0cMPI %5").arg(caseId).arg(mach).arg(aoa).arg(temperature).arg(mpi));
    });
    singleLayout->addWidget(editSingleParameters, 0, Qt::AlignLeft);
    singleParametersSummary_->setText(uiText(u"\u5f53\u524d\u5355\u6b21\u5de5\u51b5\uff1a%1\n\u9a6c\u8d6b\u6570 %2\uff0c\u653b\u89d2 %3\uff0c\u6e29\u5ea6 %4 K\uff0cMPI %5").arg(singleCaseCombo_->currentText()).arg(singleMachSpin_->value()).arg(singleAoASpin_->value()).arg(singleTemperatureSpin_->value()).arg(singleMpiSpin_->value()));
    connect(singleCaseCombo_, &QComboBox::currentTextChanged, this, [this](const QString &id) {
        applyStateToSingleCalculation(id);
    });
    applyStateToSingleCalculation(singleCaseCombo_->currentText());
    auto *singleHint = new QLabel(uiText(u"\u9009\u62e9\u5de5\u51b5\u540e\u4f1a\u8f7d\u5165\u5de5\u51b5\u53c2\u6570\uff1b\u8fd9\u91cc\u4fee\u6539\u4ec5\u4f5c\u7528\u4e8e\u672c\u6b21\u4efb\u52a1\uff0c\u4e0d\u4f1a\u6539\u52a8\u5de5\u51b5\u7ba1\u7406\u4e2d\u7684\u5de5\u51b5\u3002\u8981\u4fee\u6539\u5de5\u51b5\u672c\u8eab\u8bf7\u56de\u5230\u5de5\u51b5\u7ba1\u7406\u3002\u70b9\u51fb\u63d0\u4ea4\u540e\u4f1a\u751f\u6210 snapshot ID\u3002"), singleCalculationPage_);
    singleHint->setWordWrap(true); singleLayout->addWidget(singleHint);
    auto *singleSubmit = new QPushButton(uiText(u"\u751f\u6210\u5e76\u63d0\u4ea4\u5355\u6b21\u4efb\u52a1"), singleCalculationPage_);
    singleSubmit->setObjectName(QStringLiteral("submitSingleCalculation"));
    connect(singleSubmit, &QPushButton::clicked, this, &MainWindow::submitSingleCalculation);
    singleLayout->addWidget(singleSubmit, 0, Qt::AlignLeft);
    singleGenerationStatus_ = new QLabel(uiText(u"\u5c1a\u672a\u751f\u6210\u4efb\u52a1"), singleCalculationPage_);
    singleGenerationStatus_->setObjectName(QStringLiteral("singleGenerationStatus"));
    singleGenerationStatus_->setWordWrap(true); singleLayout->addWidget(singleGenerationStatus_);
    singleLayout->addStretch();

    batchCalculationPage_ = createWorkspacePage(
        QStringLiteral("batchCalculationPage"), uiText(u"\u6279\u91cf\u8ba1\u7b97"),
        uiText(u"\u5148\u7528 + \u6dfb\u52a0\u591a\u7ec4\u5de5\u51b5\u53c2\u6570\uff0c\u518d\u63d0\u4ea4\u4e00\u6b21\u6279\u5904\u7406\u4efb\u52a1\u3002\u9009\u62e9\u5de5\u51b5\u540e\u4fee\u6539\u7684\u6570\u503c\u53ea\u4f5c\u7528\u4e8e\u8be5\u6279\u6b21\u7684\u5b50\u4efb\u52a1\uff0c\u4e0d\u4f1a\u6539\u52a8\u5de5\u51b5\u7ba1\u7406\u3002\u5f53\u524d\u5b9e\u73b0\u662f UI/Mock \u6d41\u7a0b\u3002"));
    auto *batchCalculationLayout = qobject_cast<QVBoxLayout *>(batchCalculationPage_->layout());
    batchCalculationTable_ = new QTableWidget(0, 6, batchCalculationPage_);
    batchCalculationTable_->setObjectName(QStringLiteral("batchCalculationTable"));
    batchCalculationTable_->setHorizontalHeaderLabels({uiText(u"\u5de5\u51b5"), uiText(u"\u9a6c\u8d6b\u6570"), uiText(u"\u653b\u89d2"), uiText(u"\u6e29\u5ea6(K)"), QStringLiteral("MPI"), uiText(u"\u6821\u9a8c")});
    batchCalculationTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    batchCalculationTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    batchCalculationTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    batchCalculationTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(batchCalculationTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row < 0 || row >= batchCalculationTable_->rowCount()) return;
        QString caseId = batchCalculationTable_->item(row, 0)->text();
        double mach = batchCalculationTable_->item(row, 1)->text().toDouble();
        double aoa = batchCalculationTable_->item(row, 2)->text().toDouble();
        double temperature = batchCalculationTable_->item(row, 3)->text().toDouble();
        int mpi = batchCalculationTable_->item(row, 4)->text().toInt();
        if (!editCalculationParameters(caseId, mach, aoa, temperature, mpi)) return;
        const QStringList values = {caseId, QString::number(mach, 'f', 3), QString::number(aoa, 'f', 3),
                                    QString::number(temperature, 'f', 2), QString::number(mpi), uiText(u"\u5df2\u901a\u8fc7")};
        for (int column = 0; column < values.size(); ++column)
            batchCalculationTable_->setItem(row, column, new QTableWidgetItem(values.at(column)));
    });
    batchCalculationLayout->addWidget(batchCalculationTable_, 1);
    auto *batchButtons = new QHBoxLayout;
    auto *addBatch = new QPushButton(uiText(u"+ \u6dfb\u52a0\u5de5\u51b5"), batchCalculationPage_);
    addBatch->setObjectName(QStringLiteral("addBatchCase"));
    auto *removeBatch = new QPushButton(uiText(u"- \u79fb\u9664\u9009\u4e2d"), batchCalculationPage_);
    removeBatch->setObjectName(QStringLiteral("removeBatchCase"));
    connect(addBatch, &QPushButton::clicked, this, &MainWindow::addBatchCase);
    connect(removeBatch, &QPushButton::clicked, this, &MainWindow::removeBatchCase);
    batchButtons->addWidget(addBatch); batchButtons->addWidget(removeBatch); batchButtons->addStretch();
    batchCalculationLayout->addLayout(batchButtons);
    auto *batchOutputLayout = new QHBoxLayout;
    batchOutputLayout->addWidget(new QLabel(uiText(u"\u6279\u5904\u7406\u8f93\u51fa\u65b9\u5f0f"), batchCalculationPage_));
    auto *batchOutputMode = new QComboBox(batchCalculationPage_);
    batchOutputMode->setObjectName(QStringLiteral("batchOutputModeCombo"));
    batchOutputMode->addItem(uiText(u"\u63d0\u4ea4\u672c\u5730\u8ba1\u7b97"), QStringLiteral("local"));
    batchOutputMode->addItem(uiText(u"\u4ec5\u751f\u6210\u8f93\u5165\u6587\u4ef6"), QStringLiteral("input-only"));
    batchOutputMode->addItem(uiText(u"\u4ec5\u751f\u6210\u8ba1\u7b97\u538b\u7f29\u5305"), QStringLiteral("package-only"));
    batchOutputLayout->addWidget(batchOutputMode);
    batchOutputLayout->addStretch();
    batchCalculationLayout->addLayout(batchOutputLayout);
    auto *submitBatch = new QPushButton(uiText(u"\u751f\u6210\u5e76\u63d0\u4ea4\u6279\u91cf\u4efb\u52a1"), batchCalculationPage_);
    submitBatch->setObjectName(QStringLiteral("submitBatchCalculation"));
    connect(submitBatch, &QPushButton::clicked, this, &MainWindow::submitBatchCalculation);
    batchCalculationLayout->addWidget(submitBatch, 0, Qt::AlignLeft);
    batchGenerationStatus_ = new QLabel(uiText(u"\u8bf7\u5148\u6dfb\u52a0\u81f3\u5c11\u4e00\u7ec4\u5de5\u51b5\u3002"), batchCalculationPage_);
    batchGenerationStatus_->setObjectName(QStringLiteral("batchGenerationStatus"));
    batchGenerationStatus_->setWordWrap(true); batchCalculationLayout->addWidget(batchGenerationStatus_);
    batchCalculationLayout->addStretch();
    workspaceStack_->addWidget(singleCalculationPage_);
    workspaceStack_->addWidget(batchCalculationPage_);
    workspacePages_.insert(QStringLiteral("single-calculation"), singleCalculationPage_);
    workspacePages_.insert(QStringLiteral("batch-calculation"), batchCalculationPage_);

    batchMonitorPage_ = createWorkspacePage(QStringLiteral("batchMonitorPage"), uiText(u"\u6279\u5904\u7406\u603b\u4f53\u76d1\u63a7"),
        uiText(u"Mock \u754c\u9762\u793a\u4f8b\uff1a3 \u4e2a\u5b50\u4efb\u52a1\uff0c\u5df2\u5b8c\u6210 1\uff0c\u8fd0\u884c\u4e2d 1\uff0c\u7b49\u5f85 1\uff0c\u5931\u8d25 0\u3002\u5c1a\u672a\u63a5\u5165\u771f\u5b9e BatchRun \u6570\u636e\u3002"));
    auto *batchMonitorLayout = qobject_cast<QVBoxLayout *>(batchMonitorPage_->layout());
    batchRunProgress_ = new QProgressBar(batchMonitorPage_);
    batchRunProgress_->setObjectName(QStringLiteral("batchRunProgress"));
    batchRunProgress_->setRange(0, 3);
    batchRunProgress_->setValue(1);
    batchRunProgress_->setFormat(uiText(u"Mock \u793a\u4f8b\uff1a%v / %m \u5df2\u5b8c\u6210"));
    batchMonitorLayout->addWidget(batchRunProgress_);
    batchRunSummary_ = new QLabel(uiText(u"\u603b\u6570 3    \u8fd0\u884c\u4e2d 1    \u5df2\u5b8c\u6210 1    \u5931\u8d25 0    \u7b49\u5f85 1    \uff08Mock\uff09"), batchMonitorPage_);
    batchRunSummary_->setObjectName(QStringLiteral("batchRunSummary"));
    batchMonitorLayout->addWidget(batchRunSummary_);
    batchMonitorLayout->addStretch();
    taskMonitorPage_ = createWorkspacePage(QStringLiteral("taskMonitorPage"), uiText(u"\u5355\u4efb\u52a1\u76d1\u63a7"),
        uiText(u"\u7efc\u5408\u663e\u793a\u4efb\u52a1\u8fdb\u5ea6\u3001\u6536\u655b\u8d8b\u52bf\u548c\u8d44\u6e90\u4fe1\u606f\u3002\u672a\u63a5\u5165\u771f\u5b9e\u6570\u636e\u7684\u5b57\u6bb5\u5747\u6807\u8bb0\u4e3a Mock\u3002"));
    dataProcessPage_ = createWorkspacePage(QStringLiteral("intelligentDataPage"), uiText(u"\u667a\u80fd\u6570\u636e\u5904\u7406"),
        uiText(u"\u8bfb\u53d6\u6c42\u89e3\u5668\u4fdd\u5b58\u7684\u7ed3\u679c\u6587\u4ef6\u3002\u7ed3\u679c\u6570\u636e\u8868\u5c55\u793a\u91c7\u6837\u5e8f\u5217\u548c\u66f2\u7ebf\uff0c\u7ed3\u679c\u6c47\u603b\u8868\u6309\u5de5\u51b5\u5bf9\u6bd4\u5404\u58c1\u9762\u6c14\u52a8\u529b\u7cfb\u6570\u3002"));
    // The module page keeps its own object name and heading: the workspace-tree
    // test pins both. The two result pages live inside it on a private stack, so
    // the left tree gains two real destinations without the module turning into
    // two separate top-level modules.
    intelligentStack_ = new QStackedWidget(dataProcessPage_);
    intelligentStack_->setObjectName(QStringLiteral("intelligentDataStack"));
    resultDataTablePage_ = new ResultDataTablePage(intelligentStack_);
    resultDataTablePage_->setObjectName(QStringLiteral("resultDataTablePage"));
    resultSummaryTablePage_ = new ResultSummaryTablePage(intelligentStack_);
    resultSummaryTablePage_->setObjectName(QStringLiteral("resultSummaryTablePage"));
    intelligentStack_->addWidget(resultDataTablePage_);
    intelligentStack_->addWidget(resultSummaryTablePage_);
    intelligentPageIndex_.insert(QStringLiteral("intelligent-data"), 0);
    intelligentPageIndex_.insert(QStringLiteral("result-data-table"), 0);
    intelligentPageIndex_.insert(QStringLiteral("result-summary-table"), 1);
    qobject_cast<QVBoxLayout *>(dataProcessPage_->layout())->addWidget(intelligentStack_, 1);
    postprocessOverviewPage_ = createWorkspacePage(
        QStringLiteral("postprocessOverviewPage"), uiText(u"\u540e\u5904\u7406\u5206\u6790"),
        uiText(u"\u8be5\u6a21\u5757\u529f\u80fd\u5f85\u63a5\u5165\u3002\u5df2\u6709\u540e\u5904\u7406\u5de5\u4f5c\u53f0\u4e0e\u76f8\u5173\u4ee3\u7801\u4fdd\u6301\u4e0d\u53d8\uff0c\u672c\u9875\u4e0d\u521b\u5efa\u7b2c\u4e8c\u4e2a VTK \u89c6\u53e3\u6216\u6267\u884c\u540e\u5904\u7406\u7b97\u6cd5\u3002"));
    qobject_cast<QVBoxLayout *>(postprocessOverviewPage_->layout())->addStretch();
    // The live monitoring page is a real page fed by task events and monitor
    // samples. It sits beside the mock task-monitor page above instead of
    // replacing it: the placeholder carries its own navigation entry and the
    // window-level tests pin its object names and log behaviour.
    monitorPage_ = new ComputeMonitorPage(workspaceStack_);
    computeMonitorPage_ = monitorPage_;
    workspaceStack_->addWidget(computeMonitorPage_);
    workspacePages_.insert(QStringLiteral("compute-monitor"), computeMonitorPage_);
    for (auto *page : {parameterWorkspacePage_, calculationOverviewPage_, stateListPage_, batchPlanPage_,
                       batchMonitorPage_, taskMonitorPage_, dataProcessPage_, postprocessOverviewPage_})
        workspaceStack_->addWidget(page);
    workspacePages_.insert(QStringLiteral("viewport"), viewportPage_);
    workspacePages_.insert(QStringLiteral("trajectory-settings"), trajectorySettingsPage_);
    workspacePages_.insert(QStringLiteral("parameters"), parameterWorkspacePage_);
    workspacePages_.insert(QStringLiteral("calculation-overview"), calculationOverviewPage_);
    workspacePages_.insert(QStringLiteral("states"), stateListPage_);
    workspacePages_.insert(QStringLiteral("batch-plan"), batchPlanPage_);
    workspacePages_.insert(QStringLiteral("batch-monitor"), batchMonitorPage_);
    workspacePages_.insert(QStringLiteral("task-monitor"), taskMonitorPage_);
    workspacePages_.insert(QStringLiteral("data-process"), dataProcessPage_);
    workspacePages_.insert(QStringLiteral("intelligent-data"), dataProcessPage_);
    // Both result pages resolve to the one module page; showWorkspacePage picks
    // the inner index. Registering them here is what makes the shared
    // navigation dispatch in handleNavigationSelection find them.
    workspacePages_.insert(QStringLiteral("result-data-table"), dataProcessPage_);
    workspacePages_.insert(QStringLiteral("result-summary-table"), dataProcessPage_);
    workspacePages_.insert(QStringLiteral("postprocess-overview"), postprocessOverviewPage_);
    const QString preprocessModuleId = QStringLiteral("module-preprocess");
    const QString parameterModuleId = QStringLiteral("module-parameters");
    const QString calculationModuleId = QStringLiteral("module-calculation");
    const QString intelligentModuleId = QStringLiteral("module-intelligent-data");
    const QString postprocessModuleId = QStringLiteral("module-postprocess-analysis");
    for (const QString &pageId : {QStringLiteral("viewport"), QStringLiteral("trajectory-settings")})
        workspacePageModules_.insert(pageId, preprocessModuleId);
    workspacePageModules_.insert(QStringLiteral("parameters"), parameterModuleId);
    for (const QString &pageId : {QStringLiteral("calculation-overview"), QStringLiteral("states"),
                                  QStringLiteral("batch-plan"), QStringLiteral("batch-monitor"),
                                  QStringLiteral("task-monitor"), QStringLiteral("single-calculation"),
                                  QStringLiteral("batch-calculation")})
        workspacePageModules_.insert(pageId, calculationModuleId);
    workspacePageModules_.insert(QStringLiteral("data-process"), intelligentModuleId);
    workspacePageModules_.insert(QStringLiteral("intelligent-data"), intelligentModuleId);
    workspacePageModules_.insert(QStringLiteral("result-data-table"), intelligentModuleId);
    workspacePageModules_.insert(QStringLiteral("result-summary-table"), intelligentModuleId);
    workspacePageModules_.insert(QStringLiteral("postprocess-overview"), postprocessModuleId);
    moduleLastPages_.insert(preprocessModuleId, QStringLiteral("viewport"));
    moduleLastPages_.insert(parameterModuleId, QStringLiteral("parameters"));
    moduleLastPages_.insert(calculationModuleId, QStringLiteral("calculation-overview"));
    moduleLastPages_.insert(intelligentModuleId, QStringLiteral("intelligent-data"));
    moduleLastPages_.insert(postprocessModuleId, QStringLiteral("postprocess-overview"));
    setCentralWidget(workspaceStack_);

    projectDock_ = new QDockWidget(uiText(u"\u5de5\u4f5c\u533a\u6811"), this);
    projectDock_->setObjectName(QStringLiteral("projectDock"));
    projectDock_->setMinimumWidth(220);
    projectDock_->setFeatures(QDockWidget::DockWidgetMovable);
    projectDock_->setAllowedAreas(Qt::LeftDockWidgetArea);
    projectDock_->installEventFilter(this);
    projectTree_ = new QTreeWidget(projectDock_);
    projectTree_->setObjectName(QStringLiteral("workspaceTree"));
    projectTree_->setHeaderHidden(true);
    auto *root = new QTreeWidgetItem(projectTree_, {uiText(u"\u5de5\u4f5c\u533a Workspace\uff08\u672a\u6253\u5f00\uff09")});
    root->setData(0, NodeTypeRole, QStringLiteral("workspace-root"));
    auto *preprocessModule = addNavigationItem(root, uiText(u"\u524d\u5904\u7406"), {}, QStringLiteral("module-preprocess"), QStringLiteral("module"));
    // Two leaf nodes only: the former mesh/trajectory sub-pages are consolidated
    // into the two pages, so these nodes must not expose an expand indicator.
    addNavigationItem(preprocessModule, uiText(u"\u7f51\u683c\u7ba1\u7406"),
                      QStringLiteral("viewport"), QStringLiteral("mesh-management"), QStringLiteral("feature"));
    addNavigationItem(preprocessModule, uiText(u"\u5f39\u9053\u8bbe\u7f6e"),
                      QStringLiteral("trajectory-settings"), QStringLiteral("trajectory-settings"), QStringLiteral("feature"));
    auto *parameterModule = addNavigationItem(root, uiText(u"\u53c2\u6570\u8bbe\u7f6e"), {}, QStringLiteral("module-parameters"), QStringLiteral("module"));
    const struct ParameterNode { const char *id; const char16_t *label; } parameterNodes[] = {
        {"control-equations", u"\u63a7\u5236\u65b9\u7a0b"}, {"boundary-conditions", u"\u8fb9\u754c\u6761\u4ef6"},
        {"mesh-motion", u"\u7f51\u683c\u8fd0\u52a8"}, {"solution-method", u"\u6c42\u89e3\u65b9\u6cd5"},
        {"reference-values", u"\u53c2\u8003\u91cf"}, {"output-control", u"\u8f93\u51fa\u63a7\u5236"},
        {"initialization", u"\u521d\u59cb\u5316"}, {"calculation-control", u"\u8ba1\u7b97\u63a7\u5236"}
    };
    for (const auto &definition : parameterNodes) {
        addNavigationItem(parameterModule,
            QString::fromUtf16(reinterpret_cast<const ushort *>(definition.label)),
            QStringLiteral("parameter-dialog"), QString::fromLatin1(definition.id), QStringLiteral("parameter-page"));
    }
    auto *calculationModule = addNavigationItem(root, uiText(u"\u8ba1\u7b97"), {}, QStringLiteral("module-calculation"), QStringLiteral("module"));
    auto *startCalculation = addNavigationItem(calculationModule, uiText(u"\u5f00\u59cb\u8ba1\u7b97"), QStringLiteral("calculation-overview"), QStringLiteral("start-calculation"), QStringLiteral("group"));
    statesNavigationItem_ = addNavigationItem(startCalculation, uiText(u"\u5de5\u51b5\u7ba1\u7406"), QStringLiteral("state-manager"), QStringLiteral("states"), QStringLiteral("state-manager"));
    for (const QString &id : stateIds_) {
        addNavigationItem(statesNavigationItem_, id + uiText(u"\uff08Mock \u754c\u9762\u793a\u4f8b\uff09"), QStringLiteral("state"), id, QStringLiteral("state"));
    }
    auto *plans = addNavigationItem(startCalculation, uiText(u"\u6279\u5904\u7406\u8ba1\u5212"), QStringLiteral("batch-plans"), QStringLiteral("batch-plans"), QStringLiteral("batch-plans"));
    plans->setToolTip(0, uiText(u"\u53ea\u8bfb\u9884\u7559\uff1a\u5f53\u524d\u8bf7\u4f7f\u7528\u6279\u91cf\u8ba1\u7b97\u76f4\u63a5\u7ec4\u7ec7\u5e76\u63d0\u4ea4\u4efb\u52a1"));
    addNavigationItem(startCalculation, uiText(u"\u5355\u6b21\u8ba1\u7b97"), QStringLiteral("single-calculation"), QStringLiteral("single-calculation"), QStringLiteral("feature"));
    addNavigationItem(startCalculation, uiText(u"\u6279\u91cf\u8ba1\u7b97"), QStringLiteral("batch-calculation"), QStringLiteral("batch-calculation"), QStringLiteral("feature"));
    auto *calculationMonitor = addNavigationItem(calculationModule, uiText(u"\u8ba1\u7b97\u76d1\u63a7"), QStringLiteral("task-monitor"), QStringLiteral("calculation-monitor"), QStringLiteral("group"));
    auto *singleRuns = addNavigationItem(calculationMonitor, uiText(u"\u5355\u6b21\u8ba1\u7b97\u4efb\u52a1"), {}, QStringLiteral("single-runs"), QStringLiteral("group"));
    auto *run = addNavigationItem(singleRuns, uiText(u"Run_001\uff08Mock \u793a\u4f8b\uff09"), QStringLiteral("task"), QStringLiteral("Run_001"), QStringLiteral("task"));
    setTaskItemStatus(run, TaskStatus::Running);
    auto *batchRuns = addNavigationItem(calculationMonitor, uiText(u"\u6279\u5904\u7406\u4efb\u52a1"), {}, QStringLiteral("batch-runs"), QStringLiteral("group"));
    auto *batchRun = addNavigationItem(batchRuns, uiText(u"BatchRun_001\uff08Mock \u793a\u4f8b\uff09"), QStringLiteral("batch-run"), QStringLiteral("BatchRun_001"), QStringLiteral("batch-run"));
    setTaskItemStatus(batchRun, TaskStatus::Running);
    for (int i = 1; i <= 3; ++i) {
        const QString id = QStringLiteral("Task_%1").arg(i, 3, 10, QLatin1Char('0'));
        auto *task = addNavigationItem(batchRun, id + uiText(u"\uff08Mock \u793a\u4f8b\uff09"), QStringLiteral("task"), id, QStringLiteral("task"));
        setTaskItemStatus(task, i == 1 ? TaskStatus::Finished : i == 2 ? TaskStatus::Running : TaskStatus::Preparing);
    }
    // Appended after the two run groups, never between them: the workspace tree
    // tests address 单次计算任务 and 批处理任务 by child index under 计算监控.
    addNavigationItem(calculationMonitor, uiText(u"\u5b9e\u65f6\u76d1\u63a7"), QStringLiteral("compute-monitor"), QStringLiteral("compute-monitor"), QStringLiteral("feature"));
    auto *intelligentModule = addNavigationItem(root, uiText(u"\u667a\u80fd\u6570\u636e\u5904\u7406"), {}, QStringLiteral("module-intelligent-data"), QStringLiteral("module"));
    addNavigationItem(intelligentModule, uiText(u"\u7ed3\u679c\u6570\u636e\u8868"), QStringLiteral("result-data-table"), QStringLiteral("result-data-table"), QStringLiteral("feature"));
    addNavigationItem(intelligentModule, uiText(u"\u7ed3\u679c\u6c47\u603b\u8868"), QStringLiteral("result-summary-table"), QStringLiteral("result-summary-table"), QStringLiteral("feature"));
    addNavigationItem(root, uiText(u"\u540e\u5904\u7406\u5206\u6790"), {}, QStringLiteral("module-postprocess-analysis"), QStringLiteral("module"));
    root->setExpanded(true);
    preprocessModule->setExpanded(true);
    parameterModule->setExpanded(true);
    calculationModule->setExpanded(true);
    startCalculation->setExpanded(true);
    calculationMonitor->setExpanded(true);
    connect(projectTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) { handleNavigationSelection(item); });
    projectDock_->setWidget(projectTree_);

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
    // Leave enough room for the project tree to grow while keeping the
    // property editor usable beside it.
    propertyDock_->setMinimumWidth(180);
    // The property editor is part of the workbench layout.  Keeping it docked
    // prevents a close/float action from making every later selection reopen a
    // floating window and leaves the left dock splitter available for resizing.
    propertyDock_->setFeatures(QDockWidget::DockWidgetMovable);
    propertyDock_->setAllowedAreas(Qt::LeftDockWidgetArea);
    auto *properties = new QWidget(propertyDock_);
    auto *propertyLayout = new QVBoxLayout(properties);
    propertyTitle_ = new QLabel(properties);
    propertyTitle_->setObjectName(QStringLiteral("propertyTitle"));
    propertyLayout->addWidget(propertyTitle_);
    auto *notice = new QLabel(uiText(u"\u53c2\u6570\u8349\u7a3f\u793a\u4f8b \u00b7 \u4ec5\u4fdd\u5b58\u5728\u5f53\u524d\u7a97\u53e3\n\u672a\u6253\u5f00\u771f\u5b9e\u5de5\u4f5c\u533a\u65f6\u7f16\u8f91\u63a7\u4ef6\u9501\u5b9a"), properties);
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
    selectPage(QStringLiteral("viewport"));

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
    taskMonitorContentSplitter_ = new QSplitter(Qt::Vertical, taskMonitorPage_);
    taskMonitorContentSplitter_->setObjectName(QStringLiteral("taskMonitorContentSplitter"));
    taskMonitorContentSplitter_->setChildrenCollapsible(false);
    auto *curveFrame = new QFrame(taskMonitorContentSplitter_);
    curveFrame->setObjectName(QStringLiteral("convergenceCurvePanel"));
    curveFrame->setFrameShape(QFrame::StyledPanel);
    curveFrame->setMinimumHeight(150);
    auto *curveLayout = new QVBoxLayout(curveFrame);
    auto *curveTitle = new QLabel(uiText(u"\u6536\u655b\u66f2\u7ebf"), curveFrame);
    QFont curveTitleFont = curveTitle->font();
    curveTitleFont.setBold(true);
    curveTitle->setFont(curveTitleFont);
    curveLayout->addWidget(curveTitle);
    taskCurveState_ = emptyState(uiText(u"Task_001 \u00b7 Mock \u66f2\u7ebf\u533a\u57df\n\u7b49\u5f85\u6b8b\u5dee\u91c7\u6837\u6570\u636e\u4e0e\u66f2\u7ebf\u7ec4\u4ef6\u63a5\u5165"), curveFrame);
    taskCurveState_->setObjectName(QStringLiteral("taskCurveState"));
    curveLayout->addWidget(taskCurveState_, 1);

    auto *taskLogPanel = new QFrame(taskMonitorContentSplitter_);
    taskLogPanel->setObjectName(QStringLiteral("taskRuntimeLogPanel"));
    taskLogPanel->setFrameShape(QFrame::StyledPanel);
    taskLogPanel->setMinimumHeight(130);
    auto *taskLogLayout = new QVBoxLayout(taskLogPanel);
    taskLogLayout->setContentsMargins(8, 6, 8, 8);
    auto *taskLogToolbar = new QHBoxLayout;
    auto *taskLogTitle = new QLabel(uiText(u"\u5f53\u524d\u4efb\u52a1\u65e5\u5fd7"), taskLogPanel);
    QFont taskLogTitleFont = taskLogTitle->font();
    taskLogTitleFont.setBold(true);
    taskLogTitle->setFont(taskLogTitleFont);
    taskLogToolbar->addWidget(taskLogTitle);
    taskLogStreamFilter_ = new QComboBox(taskLogPanel);
    taskLogStreamFilter_->setObjectName(QStringLiteral("taskLogStreamFilter"));
    taskLogStreamFilter_->addItem(uiText(u"\u5168\u90e8\u8f93\u51fa"), QStringLiteral("all"));
    taskLogStreamFilter_->addItem(QStringLiteral("stdout"), QStringLiteral("stdout"));
    taskLogStreamFilter_->addItem(QStringLiteral("stderr"), QStringLiteral("stderr"));
    taskLogToolbar->addWidget(taskLogStreamFilter_);
    taskLogAutoScroll_ = new QCheckBox(uiText(u"\u81ea\u52a8\u6eda\u52a8"), taskLogPanel);
    taskLogAutoScroll_->setObjectName(QStringLiteral("taskLogAutoScroll"));
    taskLogAutoScroll_->setChecked(true);
    taskLogToolbar->addWidget(taskLogAutoScroll_);
    auto *appendTaskLog = new QPushButton(uiText(u"\u8ffd\u52a0\u6f14\u793a"), taskLogPanel);
    appendTaskLog->setObjectName(QStringLiteral("appendTaskLogMockButton"));
    appendTaskLog->setToolTip(uiText(u"Mock\uff1a\u8ffd\u52a0\u4e00\u884c stdout\uff0c\u4e0d\u63a5\u5165\u771f\u5b9e\u6c42\u89e3\u5668"));
    taskLogToolbar->addWidget(appendTaskLog);
    auto *updateProgress = new QPushButton(uiText(u"\u66f4\u65b0\u8fdb\u5ea6"), taskLogPanel);
    updateProgress->setObjectName(QStringLiteral("updateTaskProgressMockButton"));
    updateProgress->setToolTip(uiText(u"Mock\uff1a\u66ff\u6362\u5f53\u524d\u8fdb\u5ea6\u884c\uff0c\u6f14\u793a\u4e0d\u6362\u884c\u8f93\u51fa"));
    taskLogToolbar->addWidget(updateProgress);
    auto *clearTaskLog = new QPushButton(uiText(u"\u6e05\u7a7a\u663e\u793a"), taskLogPanel);
    clearTaskLog->setObjectName(QStringLiteral("clearTaskLogButton"));
    taskLogToolbar->addWidget(clearTaskLog);
    taskLogToolbar->addStretch();
    taskLogLayout->addLayout(taskLogToolbar);
    taskLogView_ = new QPlainTextEdit(taskLogPanel);
    taskLogView_->setObjectName(QStringLiteral("taskRuntimeLogView"));
    taskLogView_->setReadOnly(true);
    taskLogView_->setLineWrapMode(QPlainTextEdit::NoWrap);
    taskLogView_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    taskLogView_->setPlaceholderText(uiText(u"\u5f53\u524d\u4efb\u52a1\u6682\u65e0\u8fd0\u884c\u65e5\u5fd7\uff08Mock\uff09"));
    taskLogLayout->addWidget(taskLogView_, 1);
    taskMonitorContentSplitter_->addWidget(curveFrame);
    taskMonitorContentSplitter_->addWidget(taskLogPanel);
    taskMonitorContentSplitter_->setStretchFactor(0, 3);
    taskMonitorContentSplitter_->setStretchFactor(1, 2);
    taskMonitorContentSplitter_->setSizes({300, 170});
    taskMonitorLayout->addWidget(taskMonitorContentSplitter_, 1);
    taskResourceSummary_ = new QLabel(uiText(u"CPU 76%    \u5185\u5b58 18.3 GB    MPI 32    \u8ba1\u7b97\u65f6\u95f4 00:12:35    \uff08Mock\uff09"), taskMonitorPage_);
    taskResourceSummary_->setObjectName(QStringLiteral("taskResourceSummary"));
    taskMonitorLayout->addWidget(taskResourceSummary_);

    taskLogEntries_.insert(QStringLiteral("Run_001"), {
        {QStringLiteral("stdout"), QStringLiteral("[12:00:01] Solver started")},
        {QStringLiteral("stdout"), QStringLiteral("[12:00:02] Iteration 100: Residual = 1.0e-2")},
        {QStringLiteral("stderr"), QStringLiteral("[12:00:03] Warning: convergence is slower than expected")},
        {QStringLiteral("stdout"), QStringLiteral("Progress: 12% (same-line update mock)")}});
    taskLogEntries_.insert(QStringLiteral("Task_001"), {
        {QStringLiteral("stdout"), QStringLiteral("[12:10:01] Solver started for Task_001")},
        {QStringLiteral("stdout"), QStringLiteral("[12:10:02] Iteration 1: Residual = 0.1")},
        {QStringLiteral("stdout"), QStringLiteral("[12:10:03] Iteration 2: Residual = 0.01")},
        {QStringLiteral("stderr"), QStringLiteral("[12:10:04] Warning: convergence issue")},
        {QStringLiteral("stdout"), QStringLiteral("Progress: 100% (same-line update mock)")}});
    taskLogEntries_.insert(QStringLiteral("Task_002"), {
        {QStringLiteral("stdout"), QStringLiteral("[12:20:01] Solver started for Task_002")},
        {QStringLiteral("stdout"), QStringLiteral("[12:20:02] Iteration 250: Residual = 4.2e-3")},
        {QStringLiteral("stdout"), QStringLiteral("Progress: 42% (same-line update mock)")}});
    taskLogEntries_.insert(QStringLiteral("Task_003"), {});
    connect(taskLogStreamFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { refreshTaskLog(); });
    connect(clearTaskLog, &QPushButton::clicked, this, [this] {
        taskLogEntries_[currentTaskId_].clear();
        refreshTaskLog();
    });
    connect(appendTaskLog, &QPushButton::clicked, this, [this] {
        const int iteration = taskLogEntries_.value(currentTaskId_).size() + 1;
        appendTaskLogMock(QStringLiteral("stdout"),
            uiText(u"Mock \u8ffd\u52a0\uff1aIteration %1: Residual = 1.0e-%2").arg(iteration).arg(iteration));
    });
    connect(updateProgress, &QPushButton::clicked, this, [this] {
        const int previous = taskLogEntries_.value(currentTaskId_).size() * 10;
        updateTaskProgressMock(QStringLiteral("Progress: %1% (same-line update mock)").arg(qMin(100, previous + 10)));
    });
    showTaskMonitor(currentTaskId_);

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
        if (item) selectNavigationNode(projectTree_, item->text(0));
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
        {uiText(u"\u754c\u9762"), QStringLiteral("UI")}, {uiText(u"\u5de5\u4f5c\u533a"), QStringLiteral("Workspace")},
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
    appendLog(QStringLiteral("INFO"), QStringLiteral("Workspace"), uiText(u"Mock\uff1a\u5de5\u4f5c\u533a DemoWorkspace \u5df2\u6253\u5f00"), QStringLiteral("current-project"));
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
    if (syncingRibbonTree_) return;
    auto *category = ribbonBar()->categoryByIndex(index);
    if (!category) return;
    const QHash<QString, QString> moduleIds = {
        {QStringLiteral("preprocessCategory"), QStringLiteral("module-preprocess")},
        {QStringLiteral("parametersCategory"), QStringLiteral("module-parameters")},
        {QStringLiteral("calculationCategory"), QStringLiteral("module-calculation")},
        {QStringLiteral("intelligentDataCategory"), QStringLiteral("module-intelligent-data")},
        {QStringLiteral("postprocessAnalysisCategory"), QStringLiteral("module-postprocess-analysis")}};
    const QString moduleId = moduleIds.value(category->objectName());
    if (moduleId.isEmpty()) return;
    syncingRibbonTree_ = true;
    selectNavigationNode(projectTree_, moduleId);
    syncingRibbonTree_ = false;
    showModuleWorkspace(moduleId);
}

void MainWindow::handleNavigationSelection(QTreeWidgetItem *item)
{
    if (!item) return;
    if (!syncingRibbonTree_) {
        auto *module = item;
        while (module && module->data(0, NodeTypeRole).toString() != QStringLiteral("module")) module = module->parent();
        if (module) {
            const QHash<QString, QString> categoryNames = {
                {QStringLiteral("module-preprocess"), QStringLiteral("preprocessCategory")},
                {QStringLiteral("module-parameters"), QStringLiteral("parametersCategory")},
                {QStringLiteral("module-calculation"), QStringLiteral("calculationCategory")},
                {QStringLiteral("module-intelligent-data"), QStringLiteral("intelligentDataCategory")},
                {QStringLiteral("module-postprocess-analysis"), QStringLiteral("postprocessAnalysisCategory")}};
            if (auto *category = findChild<SARibbonCategory *>(categoryNames.value(module->data(0, ObjectIdRole).toString()))) {
                if (ribbonBar()->currentIndex() != ribbonBar()->categoryIndex(category)) {
                    syncingRibbonTree_ = true;
                    ribbonBar()->raiseCategory(category);
                    syncingRibbonTree_ = false;
                }
            }
        }
    }
    const QString page = item->data(0, PageRole).toString();
    const QString objectId = item->data(0, ObjectIdRole).toString();
    if (page.isEmpty()) {
        if (!syncingRibbonTree_) {
            auto *module = item;
            while (module && module->data(0, NodeTypeRole).toString() != QStringLiteral("module"))
                module = module->parent();
            if (module) showModuleWorkspace(module->data(0, ObjectIdRole).toString());
        }
        return;
    }
    if (page == QStringLiteral("parameter-dialog")) {
        selectPage(page + QLatin1Char('|') + objectId);
        showParameterDialog(objectId, item->text(0));
        emit navigationRequested(page, objectId, {});
        return;
    }
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
        showTaskMonitor(objectId);
    }
    else if (page == QStringLiteral("compute-monitor")) showComputeMonitor();
    else if (workspacePages_.contains(page)) showWorkspacePage(page);
    else if (page == QStringLiteral("dataset") || page == QStringLiteral("data-process")) showWorkspacePage(QStringLiteral("data-process"));
    else if (page == QStringLiteral("monitor-log")) showOutput(0);
    else if (page == QStringLiteral("monitor-residual")) showWorkspacePage(QStringLiteral("task-monitor"));
    else if (page == QStringLiteral("monitor-errors")) showOutput(3);
    else showWorkspacePage(QStringLiteral("viewport"));
}

void MainWindow::showWorkspacePage(const QString &pageId)
{
    if (auto *target = workspacePages_.value(pageId, nullptr)) {
        workspaceStack_->setCurrentWidget(target);
        // Several page ids share the module widget, so the id alone does not say
        // which screen to show; the inner stack carries that. Anything not
        // listed keeps the index it was left on.
        if (pageId == QStringLiteral("intelligent-data")
            || pageId == QStringLiteral("result-data-table")
            || pageId == QStringLiteral("result-summary-table")) {
            if (intelligentStack_) intelligentStack_->setCurrentIndex(intelligentPageIndex_.value(pageId, 0));
        }
        const QString moduleId = workspacePageModules_.value(pageId);
        if (!moduleId.isEmpty()) moduleLastPages_.insert(moduleId, pageId);
        setMeshWorkspaceActive(target == viewportPage_);
        // Embedded parameter editors fill the central area, so hide the shared
        // property dock while editing and keep the workspace tree at a stable
        // width. Leaving the parameter pages restores main's dock state.
        if (pageId.startsWith(QStringLiteral("parameter-"))) {
            if (!parameterDockHidden_) {
                propertyDockVisibleBeforeParameter_ = propertyDock_ && propertyDock_->isVisible();
                parameterDockHidden_ = true;
            }
            propertyDock_->hide();
            resizeDocks({projectDock_}, {260}, Qt::Horizontal);
        } else if (parameterDockHidden_) {
            parameterDockHidden_ = false;
            if (propertyDockVisibleBeforeParameter_ && !meshWorkspaceActive_)
                propertyDock_->show();
        }
    }
}

void MainWindow::showComputeMonitor()
{
    if (!monitorPage_) return;
    // Reuse the shared page switch instead of toggling the stack here: leaving
    // a parameter page has to restore the property dock, and that state lives in
    // showWorkspacePage().
    selectPage(QStringLiteral("compute-monitor"));
    showWorkspacePage(QStringLiteral("compute-monitor"));
}

void MainWindow::showModuleWorkspace(const QString &moduleId)
{
    const QString pageId = moduleLastPages_.value(moduleId);
    if (pageId.isEmpty() || !workspacePages_.contains(pageId)) return;
    selectPage(pageId);
    showWorkspacePage(pageId);
}

void MainWindow::setMeshWorkspaceActive(bool active)
{
    if (meshWorkspaceActive_ == active) {
        // selectPage() updates the shared property editor before routing the
        // central page, so repeated mesh navigation must still suppress it.
        if (active && propertyDock_) {
            propertyDock_->hide();
        }
        return;
    }
    if (active) {
        propertyDockVisibleBeforeMesh_ = propertyDock_ && propertyDock_->isVisible();
        meshWorkspaceActive_ = true;
        if (propertyDock_) propertyDock_->hide();
        projectDock_->setMinimumWidth(projectDockWidth_);
        resizeDocks({projectDock_}, {projectDockWidth_}, Qt::Horizontal);
        projectDock_->setMinimumWidth(220);
        // Collapse the global output once for the initial mesh layout. Later
        // navigation must preserve the user's explicit expand/collapse choice.
        if (!meshOutputInitialCollapseApplied_) {
            setOutputCollapsed(true);
            meshOutputInitialCollapseApplied_ = true;
        }
    } else {
        meshWorkspaceActive_ = false;
        if (propertyDock_ && propertyDockVisibleBeforeMesh_) {
            propertyDock_->show();
            projectDock_->setMinimumWidth(projectDockWidth_);
            resizeDocks({projectDock_, propertyDock_},
                        {projectDockWidth_, 180}, Qt::Horizontal);
            projectDock_->setMinimumWidth(220);
        }
    }
}

void MainWindow::showParameterDialog(const QString &pageId, const QString &title)
{
    const QString draftKey = pageId == "control-equations" ? QStringLiteral("physics")
        : pageId == "boundary-conditions" ? QStringLiteral("boundary")
        : pageId == "mesh-motion" ? QStringLiteral("motion") : pageId;
    selectPage(QStringLiteral("parameter-dialog|") + pageId);
    if (parameterWorkspaceTitle_)
        parameterWorkspaceTitle_->setText(title + uiText(u" \u00b7 \u53c2\u6570\u8bbe\u7f6e\uff08Mock\uff09"));
    showWorkspacePage(QStringLiteral("parameters"));
    const QString workspaceId = QStringLiteral("parameter-") + pageId;
    if (workspacePages_.contains(workspaceId)) {
        if (pageId == "solution-method") {
            auto *panel = workspacePages_.value(workspaceId)->findChild<SolutionMethodPanel *>();
            if (panel) panel->setUnsteady(drafts_.value("physics").value("calculationType", "steady") == "unsteady");
        }
        showWorkspacePage(workspaceId);
        return;
    }
    auto *dialog = new QWidget(workspaceStack_);
    dialog->setObjectName(QStringLiteral("parameterEditor_") + pageId);
    dialog->setProperty("parameterEditor", true);
    dialog->setAttribute(Qt::WA_StyledBackground, true);
    workspaceStack_->addWidget(dialog);
    workspacePages_.insert(workspaceId, dialog);
    workspacePageModules_.insert(workspaceId, QStringLiteral("module-parameters"));
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);
    auto *heading = new QLabel(title, dialog);
    heading->setObjectName(QStringLiteral("parameterHeading"));
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 2);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);
    auto *note = new QLabel(uiText(u"\u524d\u7aef\u914d\u7f6e\u5360\u4f4d\uff1a\u5f53\u524d\u4e0d\u6267\u884c\u4e1a\u52a1\u6821\u9a8c\u3001\u6301\u4e45\u5316\u6216\u6c42\u89e3\u5668\u66f4\u65b0\u3002"), dialog);
    note->setWordWrap(true);
    note->setProperty("parameterHint", true);
    layout->addWidget(note);
    auto *scroll = new QScrollArea(dialog);
    scroll->setObjectName(QStringLiteral("parameterScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->viewport()->setProperty("parameterSurface", true);
    scroll->viewport()->setAttribute(Qt::WA_StyledBackground, true);
    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("parameterContent"));
    content->setProperty("parameterSurface", true);
    content->setAttribute(Qt::WA_StyledBackground, true);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(16);
    auto *group = new QGroupBox(title + uiText(u"\u9009\u9879"), content);
    auto *form = new QFormLayout(group);
    const auto addChoice = [form, group](const QString &label, const QStringList &choices) {
        auto *editor = new QComboBox(group);
        editor->addItems(choices);
        form->addRow(label, editor);
    };
    const auto addValue = [form, group](const QString &label, double value, const QString &suffix) {
        auto *editor = new QDoubleSpinBox(group);
        editor->setRange(-1e9, 1e9);
        editor->setDecimals(5);
        editor->setValue(value);
        editor->setSuffix(suffix);
        form->addRow(label, editor);
    };
    const auto bindPanel = [contentLayout](QWidget *panel) {
        contentLayout->addWidget(panel);
    };
    if (pageId == QStringLiteral("control-equations")) {
        auto *panel = new GoverningEquationsPanel(drafts_.value(draftKey), content);
        connect(panel, &GoverningEquationsPanel::draftChanged, this, [this, draftKey](const QVariantMap &draft) {
            drafts_[draftKey] = draft; emit parameterDraftChanged(draftKey, draft);
        });
        bindPanel(panel);
    } else if (pageId == QStringLiteral("boundary-conditions")) {
        auto *panel = new BoundaryConditionsPanel(drafts_.value(draftKey), content);
        connect(panel, &BoundaryConditionsPanel::draftChanged, this, [this, draftKey](const QVariantMap &draft) {
            drafts_[draftKey] = draft; emit parameterDraftChanged(draftKey, draft);
        });
        bindPanel(panel);
    } else if (pageId == QStringLiteral("mesh-motion")) {
        auto *panel = new MeshMotionPanel(drafts_.value(draftKey), content);
        connect(panel, &MeshMotionPanel::draftChanged, this, [this, draftKey](const QVariantMap &draft) {
            drafts_[draftKey] = draft; emit parameterDraftChanged(draftKey, draft);
        });
        bindPanel(panel);
    } else if (pageId == QStringLiteral("calculation-control")) {
        auto *panel = new CalculationControlPanel(drafts_.value(draftKey), content);
        connect(panel, &CalculationControlPanel::draftChanged, this, [this, draftKey](const QVariantMap &draft) {
            drafts_[draftKey] = draft; emit parameterDraftChanged(draftKey, draft);
        });
        bindPanel(panel);
    } else if (pageId == QStringLiteral("solution-method")) {
        auto *panel = new SolutionMethodPanel(drafts_.value(draftKey), content);
        panel->setUnsteady(drafts_.value("physics").value("calculationType", "steady") == "unsteady");
        connect(panel, &SolutionMethodPanel::draftChanged, this, [this, draftKey](const QVariantMap &draft) {
            drafts_[draftKey] = draft; emit parameterDraftChanged(draftKey, draft);
        });
        connect(this, &MainWindow::parameterDraftChanged, panel, [panel](const QString &key, const QVariantMap &draft) {
            if (key == "physics") panel->setUnsteady(draft.value("calculationType", "steady") == "unsteady");
        });
        bindPanel(panel);
    } else if (pageId == QStringLiteral("reference-values")) {
        auto *panel = new ReferenceValuesPanel(drafts_.value(draftKey), content);
        connect(panel, &ReferenceValuesPanel::draftChanged, this, [this, draftKey](const QVariantMap &draft) {
            drafts_[draftKey] = draft; emit parameterDraftChanged(draftKey, draft);
        });
        bindPanel(panel);
    } else if (pageId == QStringLiteral("output-control")) {
        auto *panel = new OutputControlPanel(drafts_.value(draftKey), content);
        connect(panel, &OutputControlPanel::draftChanged, this, [this, draftKey](const QVariantMap &draft) {
            drafts_[draftKey] = draft; emit parameterDraftChanged(draftKey, draft);
        });
        bindPanel(panel);
    } else if (pageId == "initialization") {
        auto *panel = new InitializationPanel(drafts_.value(draftKey), content);
        panel->setSources(drafts_.value("boundary"), drafts_.value("reference-values"));
        connect(panel, &InitializationPanel::draftChanged, this, [this, draftKey](const QVariantMap &draft) {
            drafts_[draftKey] = draft; emit parameterDraftChanged(draftKey, draft);
        });
        connect(this, &MainWindow::parameterDraftChanged, panel, [this, panel](const QString &key, const QVariantMap &) {
            if (key == "boundary" || key == "reference-values")
                panel->setSources(drafts_.value("boundary"), drafts_.value("reference-values"));
        });
        bindPanel(panel);
    } else {
        addChoice(uiText(u"\u521d\u59cb\u5316\u65b9\u5f0f"), {uiText(u"\u81ea\u7531\u6d41\u521d\u59cb\u5316"), uiText(u"\u4ece\u7ed3\u679c\u6062\u590d\uff08\u5360\u4f4d\uff09")});
        addValue(uiText(u"\u521d\u59cb\u6e29\u5ea6"), 300, QStringLiteral(" K"));
    }
    if (form->rowCount()) contentLayout->addWidget(group);
    else delete group;
    contentLayout->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);
    auto *back = new QPushButton(uiText(u"\u8fd4\u56de\u4e09\u7ef4\u5de5\u4f5c\u533a"), dialog);
    connect(back, &QPushButton::clicked, this, [this] { showWorkspacePage(QStringLiteral("viewport")); });
    auto *footer = new QHBoxLayout;
    footer->addWidget(back);
    footer->addStretch();
    auto *reset = new QPushButton(uiText(u"\u6e05\u9664\u672c\u9875\u8349\u7a3f"), dialog);
    reset->setObjectName(QStringLiteral("resetParameterDraft"));
    connect(reset, &QPushButton::clicked, this, [this, pageId, draftKey, title, workspaceId, dialog] {
        drafts_.remove(draftKey);
        workspacePages_.remove(workspaceId);
        workspaceStack_->removeWidget(dialog);
        dialog->deleteLater();
        showParameterDialog(pageId, title);
    });
    footer->addWidget(reset);
    layout->addLayout(footer);
    showWorkspacePage(workspaceId);
}

void MainWindow::selectNavigationNode(QTreeWidget *tree, const QString &objectId)
{
    if (!tree) return;
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, ObjectIdRole).toString() != objectId) continue;
        for (auto *parent = (*it)->parent(); parent; parent = parent->parent()) parent->setExpanded(true);
        if ((*it)->data(0, NodeTypeRole).toString() == QStringLiteral("module")) (*it)->setExpanded(true);
        tree->setCurrentItem(*it);
        tree->scrollToItem(*it, QAbstractItemView::PositionAtTop);
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
        {"monitor-errors", uiText(u"\u9519\u8bef\u4fe1\u606f")}, {"results", uiText(u"\u7ed3\u679c\u4e0e\u540e\u5904\u7406")},
        {"parameters", uiText(u"\u53c2\u6570\u8bbe\u7f6e")}, {"calculation-overview", uiText(u"\u8ba1\u7b97")},
        {"intelligent-data", uiText(u"\u667a\u80fd\u6570\u636e\u5904\u7406")}, {"postprocess-overview", uiText(u"\u540e\u5904\u7406\u5206\u6790")},
        {"result-data-table", uiText(u"\u7ed3\u679c\u6570\u636e\u8868")}, {"result-summary-table", uiText(u"\u7ed3\u679c\u6c47\u603b\u8868")},
        {"compute-monitor", uiText(u"实时监控")},
        {"viewport", uiText(u"\u7f51\u683c\u7ba1\u7406")}, {"trajectory-settings", uiText(u"\u5f39\u9053\u8bbe\u7f6e")},
        {"single-calculation", uiText(u"\u5355\u6b21\u8ba1\u7b97")},
        {"batch-calculation", uiText(u"\u6279\u91cf\u8ba1\u7b97")}};
    const QHash<QString, QString> parameterTitles = {
        {"calculation-control", uiText(u"\u8ba1\u7b97\u63a7\u5236")},
        {"control-equations", uiText(u"\u63a7\u5236\u65b9\u7a0b")}, {"boundary-conditions", uiText(u"\u8fb9\u754c\u6761\u4ef6")},
        {"mesh-motion", uiText(u"\u7f51\u683c\u8fd0\u52a8")}, {"solution-method", uiText(u"\u6c42\u89e3\u65b9\u6cd5")},
        {"reference-values", uiText(u"\u53c2\u8003\u91cf")}, {"output-control", uiText(u"\u8f93\u51fa\u63a7\u5236")},
        {"initialization", uiText(u"\u521d\u59cb\u5316")}};
    const QString visibleTitle = pageType == QStringLiteral("parameter-dialog")
        ? parameterTitles.value(objectId, objectId) : titles.value(pageType, pageType);
    propertyTitle_->setText(visibleTitle + (objectId.isEmpty() || pageType == QStringLiteral("parameter-dialog")
        ? QString() : uiText(u" \u00b7 ") + objectId));
    const auto addNumber = [this, pageId](const QString &key, const QString &label, double value, const QString &unit) {
        auto *editor = new QDoubleSpinBox(this);
        editor->setObjectName(key);
        editor->setDecimals(4);
        editor->setRange(-1e9, 1e9);
        editor->setSuffix(unit);
        editor->setValue(drafts_.value(pageId).value(key, value).toDouble());
        editor->setEnabled(projectEditingEnabled_);
        if (!projectEditingEnabled_) editor->setToolTip(uiText(u"\u8bf7\u5148\u901a\u8fc7\u5de5\u4f5c\u533a\u670d\u52a1\u6253\u5f00\u771f\u5b9e\u5de5\u4f5c\u533a"));
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
        auto *note = new QLabel(uiText(u"\u5bf9\u8c61\uff1a%1\n\u5f53\u524d\u4ec5\u5c55\u793a Mock / \u5360\u4f4d\u4fe1\u606f\uff0c\u5c1a\u672a\u4ece\u5de5\u4f5c\u533a\u3001\u4efb\u52a1\u6216\u6570\u636e\u670d\u52a1\u52a0\u8f7d\u3002").arg(objectId), this);
        note->setWordWrap(true);
        propertyForm_->addRow(note);
    } else {
        auto *note = new QLabel(pageType == "results" ? uiText(u"\u5c1a\u65e0\u7ed3\u679c\u6570\u636e\u3002\u7ed3\u679c\u5339\u914d\u6821\u9a8c\u548c\u663e\u793a\u7531\u5de5\u4f5c\u6d41\u53ca\u53ef\u89c6\u5316\u6a21\u5757\u63d0\u4f9b\u3002") : uiText(u"\u4e00\u6b21\u8ba1\u7b97\u5bf9\u5e94\u4e00\u4e2a\u5de5\u51b5\uff0c\u5de5\u51b5\u5f52\u5c5e\u5f53\u524d\u5de5\u4f5c\u533a\u3002\u6279\u5904\u7406\u8ba1\u5212\u53ea\u5f15\u7528\u5de5\u51b5\uff0c\u5f53\u524d\u4ec5\u4e3a\u754c\u9762\u793a\u4f8b\u3002"), this);
        note->setWordWrap(true);
        propertyForm_->addRow(note);
    }
    if (meshWorkspaceActive_) propertyDock_->hide();
    else propertyDock_->show();
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
    expandedOutputHeight_ = 140;
    ribbonBar()->setMinimumMode(false);
    if (auto *meshPage = qobject_cast<MeshManagementPage *>(viewportPage_))
        meshPage->restoreDefaultLayout();
    if (taskMonitorContentSplitter_) taskMonitorContentSplitter_->setSizes({300, 170});
    if (meshWorkspaceActive_) {
        propertyDock_->hide();
        resizeDocks({projectDock_}, {260}, Qt::Horizontal);
        projectDockWidth_ = projectDock_->width();
    }
    setOutputCollapsed(true);
    if (status_) statusBar()->showMessage(uiText(u"\u5df2\u6062\u590d\u9ed8\u8ba4\u754c\u9762\u5e03\u5c40\uff1b\u5df2\u4fdd\u5b58\u7684\u7528\u6237\u5e03\u5c40\u672a\u5220\u9664"), 4000);
}

void MainWindow::saveUserLayout()
{
    QSettings settings(QStringLiteral("AeroSimWorkbench"), QStringLiteral("UiLayout"));
    settings.beginGroup(QStringLiteral("UserLayout"));
    settings.remove(QString());
    settings.setValue(QStringLiteral("version"), 1);
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("mainWindowState"), saveState(1));
    settings.setValue(QStringLiteral("maximized"), isMaximized());
    settings.setValue(QStringLiteral("ribbonMinimum"), ribbonBar()->isMinimumMode());
    settings.setValue(QStringLiteral("projectDockRatio"), double(projectDock_->width()) / qMax(1, width()));
    settings.setValue(QStringLiteral("propertyDockRatio"), double(propertyDock_->width()) / qMax(1, width()));
    settings.setValue(QStringLiteral("outputHeightRatio"), double(outputDock_->height()) / qMax(1, height()));
    settings.setValue(QStringLiteral("expandedOutputRatio"), double(expandedOutputHeight_) / qMax(1, height()));
    settings.setValue(QStringLiteral("outputCollapsed"), outputCollapsed_);
    const QList<int> monitorSizes = taskMonitorContentSplitter_->sizes();
    const int monitorTotal = monitorSizes.value(0) + monitorSizes.value(1);
    settings.setValue(QStringLiteral("monitorCurveRatio"), monitorTotal > 0
        ? double(monitorSizes.value(0)) / monitorTotal : 0.64);
    if (auto *meshPage = qobject_cast<MeshManagementPage *>(viewportPage_))
        settings.setValue(QStringLiteral("meshLayout"), meshPage->layoutState());

    QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect available = screen->availableGeometry();
        const QRect windowRect = isMaximized() ? normalGeometry() : geometry();
        settings.setValue(QStringLiteral("windowWidthRatio"), double(windowRect.width()) / qMax(1, available.width()));
        settings.setValue(QStringLiteral("windowHeightRatio"), double(windowRect.height()) / qMax(1, available.height()));
        settings.setValue(QStringLiteral("windowXRatio"), double(windowRect.left() - available.left()) / qMax(1, available.width()));
        settings.setValue(QStringLiteral("windowYRatio"), double(windowRect.top() - available.top()) / qMax(1, available.height()));
    }
    settings.endGroup();
    settings.sync();
    if (restoreUserLayoutAction_) restoreUserLayoutAction_->setEnabled(settings.status() == QSettings::NoError);
    statusBar()->showMessage(settings.status() == QSettings::NoError
        ? uiText(u"\u5f53\u524d\u5e03\u5c40\u5df2\u4fdd\u5b58")
        : uiText(u"\u5e03\u5c40\u4fdd\u5b58\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u7528\u6237\u914d\u7f6e\u76ee\u5f55"), 4000);
}

void MainWindow::restoreUserLayout()
{
    QSettings settings(QStringLiteral("AeroSimWorkbench"), QStringLiteral("UiLayout"));
    settings.beginGroup(QStringLiteral("UserLayout"));
    if (settings.value(QStringLiteral("version")).toInt() != 1) {
        settings.endGroup();
        statusBar()->showMessage(uiText(u"\u6682\u65e0\u5df2\u4fdd\u5b58\u7684\u7528\u6237\u5e03\u5c40"), 4000);
        if (restoreUserLayoutAction_) restoreUserLayoutAction_->setEnabled(false);
        return;
    }
    const QByteArray geometryState = settings.value(QStringLiteral("geometry")).toByteArray();
    const QByteArray mainWindowState = settings.value(QStringLiteral("mainWindowState")).toByteArray();
    const bool maximized = settings.value(QStringLiteral("maximized")).toBool();
    const bool ribbonMinimum = settings.value(QStringLiteral("ribbonMinimum")).toBool();
    const double projectRatio = qBound(0.12, settings.value(QStringLiteral("projectDockRatio"), 0.18).toDouble(), 0.35);
    const double propertyRatio = qBound(0.18, settings.value(QStringLiteral("propertyDockRatio"), 0.23).toDouble(), 0.38);
    const double outputRatio = qBound(0.10, settings.value(QStringLiteral("outputHeightRatio"), 0.15).toDouble(), 0.45);
    const double expandedOutputRatio = qBound(0.10, settings.value(QStringLiteral("expandedOutputRatio"), 0.15).toDouble(), 0.45);
    const bool outputCollapsed = settings.value(QStringLiteral("outputCollapsed")).toBool();
    const double monitorCurveRatio = qBound(0.45, settings.value(QStringLiteral("monitorCurveRatio"), 0.64).toDouble(), 0.82);
    const QVariantMap meshLayout = settings.value(QStringLiteral("meshLayout")).toMap();
    const double widthRatio = qBound(0.45, settings.value(QStringLiteral("windowWidthRatio"), 0.75).toDouble(), 1.0);
    const double heightRatio = qBound(0.50, settings.value(QStringLiteral("windowHeightRatio"), 0.88).toDouble(), 1.0);
    const double xRatio = qBound(0.0, settings.value(QStringLiteral("windowXRatio"), 0.1).toDouble(), 0.95);
    const double yRatio = qBound(0.0, settings.value(QStringLiteral("windowYRatio"), 0.05).toDouble(), 0.95);
    settings.endGroup();

    const QByteArray previousGeometry = saveGeometry();
    const QByteArray previousWindowState = saveState(1);
    if (geometryState.isEmpty() || mainWindowState.isEmpty()
        || !restoreGeometry(geometryState) || !restoreState(mainWindowState, 1)) {
        restoreGeometry(previousGeometry);
        restoreState(previousWindowState, 1);
        statusBar()->showMessage(uiText(u"\u5df2\u4fdd\u5b58\u7684\u5e03\u5c40\u65e0\u6548\uff0c\u5df2\u4fdd\u7559\u5f53\u524d\u754c\u9762"), 5000);
        return;
    }
    QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!maximized && screen) {
        const QRect available = screen->availableGeometry();
        const int targetWidth = qMin(available.width(), qMax(minimumWidth(), qRound(available.width() * widthRatio)));
        const int targetHeight = qMin(available.height(), qMax(minimumHeight(), qRound(available.height() * heightRatio)));
        const int maxX = available.right() - targetWidth + 1;
        const int maxY = available.bottom() - targetHeight + 1;
        const int targetX = maxX <= available.left() ? available.left()
            : qBound(available.left(), available.left() + qRound(available.width() * xRatio), maxX);
        const int targetY = maxY <= available.top() ? available.top()
            : qBound(available.top(), available.top() + qRound(available.height() * yRatio), maxY);
        showNormal();
        setGeometry(targetX, targetY, targetWidth, targetHeight);
    } else if (maximized) {
        showMaximized();
    }
    ribbonBar()->setMinimumMode(ribbonMinimum);
    expandedOutputHeight_ = qMax(120, qRound(height() * expandedOutputRatio));
    setOutputCollapsed(outputCollapsed);
    if (auto *meshPage = qobject_cast<MeshManagementPage *>(viewportPage_))
        meshPage->restoreLayoutState(meshLayout);
    taskMonitorContentSplitter_->setSizes({qRound(monitorCurveRatio * 1000), qRound((1.0 - monitorCurveRatio) * 1000)});
    QTimer::singleShot(0, this, [this, projectRatio, propertyRatio, outputRatio, outputCollapsed] {
        // Older saved layouts may contain a floating/closed property dock.
        // Normalize it once so the editor remains part of the left workbench
        // and the project/property splitter is always available.
        propertyDock_->setFloating(false);
        if (dockWidgetArea(propertyDock_) != Qt::LeftDockWidgetArea) {
            removeDockWidget(propertyDock_);
            addDockWidget(Qt::LeftDockWidgetArea, propertyDock_);
            splitDockWidget(projectDock_, propertyDock_, Qt::Horizontal);
        }
        if (!meshWorkspaceActive_) propertyDock_->show();
        resizeDocks({projectDock_, propertyDock_},
                    {qMax(220, qRound(width() * projectRatio)), qMax(300, qRound(width() * propertyRatio))},
                    Qt::Horizontal);
        projectDockWidth_ = qMax(220, projectDock_->width());
        if (!outputCollapsed)
            resizeDocks({outputDock_}, {qMax(120, qRound(height() * outputRatio))}, Qt::Vertical);
        constrainToAvailableScreen();
    });
    statusBar()->showMessage(uiText(u"\u5df2\u6062\u590d\u4e0a\u6b21\u4fdd\u5b58\u7684\u7528\u6237\u5e03\u5c40"), 4000);
}

void MainWindow::constrainToAvailableScreen()
{
    if (isMaximized()) return;
    QScreen *target = nullptr;
    for (auto *screen : QGuiApplication::screens()) {
        if (screen->availableGeometry().intersects(frameGeometry())) {
            target = screen;
            break;
        }
    }
    if (!target) target = QGuiApplication::primaryScreen();
    if (!target) return;
    const QRect available = target->availableGeometry();
    const int targetWidth = qMin(available.width(), qMax(minimumWidth(), width()));
    const int targetHeight = qMin(available.height(), qMax(minimumHeight(), height()));
    const int maxX = available.right() - targetWidth + 1;
    const int maxY = available.bottom() - targetHeight + 1;
    const int targetX = maxX <= available.left() ? available.left() : qBound(available.left(), x(), maxX);
    const int targetY = maxY <= available.top() ? available.top() : qBound(available.top(), y(), maxY);
    setGeometry(targetX, targetY, targetWidth, targetHeight);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == projectDock_ && event && event->type() == QEvent::Resize
        && QApplication::mouseButtons() != Qt::NoButton) {
        projectDockWidth_ = qMax(220, projectDock_->width());
    }
    return SARibbonMainWindow::eventFilter(watched, event);
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
        serviceConnections_.append(connect(workflow_, &IWorkflowService::workflowViewV2Changed, this,
            &MainWindow::displayWorkflow, Qt::QueuedConnection));
        // viewV2()/requestRun() are synchronous contracts; do not call across threads.
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
    if (workflow_ && workflow_->thread() == thread()) {
        const auto current = workflow_->viewV2(runRequest_.scope);
        if (current.ok()) displayWorkflow(current.value());
    }
}

void MainWindow::setRunContext(const RunRequest &request)
{
    workflowView_ = {};
    runRequest_ = request;
    hasRunContext_ = !request.inputSnapshot.snapshotId.isEmpty();
    if (workflow_ && workflow_->thread() == thread()) {
        const auto current = workflow_->viewV2(request.scope);
        if (current.ok()) displayWorkflow(current.value());
    }
}

void MainWindow::displayWorkflow(const WorkflowViewV2 &view)
{
    if (hasRunContext_ && (view.scope.projectId != runRequest_.scope.projectId || view.scope.caseId != runRequest_.scope.caseId)) return;
    if (view.scope.projectId == workflowView_.scope.projectId && view.scope.caseId == workflowView_.scope.caseId
        && view.viewRevision < workflowView_.viewRevision) return;
    workflowView_ = view;
    workflowTree_->clear();
    suggestion_->setText(view.nextSuggestion);
    if (runAction_) runAction_->setEnabled(false);
    for (const auto &step : view.steps) {
        auto *item = new QTreeWidgetItem(workflowTree_, {step.summary, stepStatus(step)});
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

void MainWindow::submitSingleCalculation()
{
    if (!workflow_ || workflow_->thread() != thread()) {
        const QString message = uiText(u"\u5de5\u4f5c\u6d41\u670d\u52a1\u5c1a\u672a\u8fde\u63a5\uff0c\u672a\u751f\u6210\u4efb\u52a1");
        if (singleGenerationStatus_) singleGenerationStatus_->setText(message);
        appendLog(QStringLiteral("ERROR"), QStringLiteral("UI"), message);
        return;
    }

    const QString caseId = singleCaseCombo_->currentText();
    const QString snapshotId = QStringLiteral("ui-%1-m%2-a%3-t%4-rev-1")
        .arg(caseId)
        .arg(singleMachSpin_->value(), 0, 'f', 3)
        .arg(singleAoASpin_->value(), 0, 'f', 3)
        .arg(singleTemperatureSpin_->value(), 0, 'f', 2);
    RunRequest request;
    request.scope.projectId = QStringLiteral("mock-project");
    request.scope.caseId = caseId;
    request.inputSnapshot.snapshotId = snapshotId;
    request.inputSnapshot.scope = request.scope;
    request.inputSnapshot.revision = 1;
    runRequest_ = request;
    hasRunContext_ = true;
    const auto result = workflow_->requestRun(request);
    if (result.allowed) {
        const QString message = uiText(u"\u5df2\u751f\u6210\u5355\u6b21\u4efb\u52a1\u8bf7\u6c42\uff1a%1\uff08Mock\uff09").arg(snapshotId);
        singleGenerationStatus_->setText(message + uiText(u"\uff1b\u8bf7\u5728\u8ba1\u7b97\u76d1\u63a7\u4e2d\u67e5\u770b\u4efb\u52a1\u4e8b\u4ef6"));
        appendMessage(message);
    } else {
        singleGenerationStatus_->setText(uiText(u"\u5355\u6b21\u4efb\u52a1\u6821\u9a8c\u5931\u8d25"));
        for (const auto &issue : result.issues)
            appendLog(QStringLiteral("ERROR"), QStringLiteral("Validation"), issue.message);
    }
}

bool MainWindow::editCalculationParameters(QString &caseId, double &mach, double &aoa,
                                            double &temperature, int &mpi)
{
    QDialog dialog(this);
    dialog.setWindowTitle(uiText(u"\u8ba1\u7b97\u53c2\u6570"));
    auto *form = new QFormLayout(&dialog);
    auto *caseCombo = new QComboBox(&dialog);
    caseCombo->addItems(stateIds_);
    caseCombo->setCurrentText(caseId);
    auto *machEditor = new QDoubleSpinBox(&dialog); machEditor->setRange(0.01, 20.0); machEditor->setValue(mach); machEditor->setSingleStep(0.05);
    auto *aoaEditor = new QDoubleSpinBox(&dialog); aoaEditor->setRange(-90.0, 90.0); aoaEditor->setValue(aoa); aoaEditor->setSingleStep(0.5);
    auto *temperatureEditor = new QDoubleSpinBox(&dialog); temperatureEditor->setRange(100.0, 2000.0); temperatureEditor->setValue(temperature); temperatureEditor->setSuffix(QStringLiteral(" K"));
    auto *mpiEditor = new QSpinBox(&dialog); mpiEditor->setRange(1, 4096); mpiEditor->setValue(mpi);
    form->addRow(uiText(u"\u5de5\u51b5"), caseCombo);
    form->addRow(uiText(u"\u9a6c\u8d6b\u6570"), machEditor);
    form->addRow(uiText(u"\u653b\u89d2"), aoaEditor);
    form->addRow(uiText(u"\u6e29\u5ea6"), temperatureEditor);
    form->addRow(QStringLiteral("MPI"), mpiEditor);
    connect(caseCombo, &QComboBox::currentTextChanged, &dialog, [this, machEditor, aoaEditor, temperatureEditor](const QString &id) {
        const auto draft = stateDrafts_.value(id);
        machEditor->setValue(draft.mach);
        aoaEditor->setValue(draft.angleOfAttack);
        temperatureEditor->setValue(draft.temperature);
    });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return false;
    caseId = caseCombo->currentText();
    mach = machEditor->value();
    aoa = aoaEditor->value();
    temperature = temperatureEditor->value();
    mpi = mpiEditor->value();
    return true;
}

void MainWindow::addBatchCase()
{
    QString caseId = stateIds_.value(0);
    const auto state = stateDrafts_.value(caseId);
    double mach = state.mach;
    double aoa = state.angleOfAttack;
    double temperature = state.temperature;
    int mpi = 4;
    if (!editCalculationParameters(caseId, mach, aoa, temperature, mpi)) return;
    const int row = batchCalculationTable_->rowCount();
    batchCalculationTable_->insertRow(row);
    const QStringList values = {
        caseId, QString::number(mach, 'f', 3), QString::number(aoa, 'f', 3),
        QString::number(temperature, 'f', 2), QString::number(mpi), uiText(u"\u5df2\u901a\u8fc7")};
    for (int column = 0; column < values.size(); ++column)
        batchCalculationTable_->setItem(row, column, new QTableWidgetItem(values.at(column)));
    batchCalculationTable_->selectRow(row);
    batchGenerationStatus_->setText(uiText(u"\u5df2\u7f16\u8f91 %1 \u7ec4\u5de5\u51b5\uff0c\u53ef\u7ee7\u7eed\u6dfb\u52a0\u6216\u63d0\u4ea4\u3002").arg(batchCalculationTable_->rowCount()));
}

void MainWindow::removeBatchCase()
{
    const int row = batchCalculationTable_ ? batchCalculationTable_->currentRow() : -1;
    if (row < 0) return;
    batchCalculationTable_->removeRow(row);
    batchGenerationStatus_->setText(batchCalculationTable_->rowCount() == 0
        ? uiText(u"\u8bf7\u5148\u6dfb\u52a0\u81f3\u5c11\u4e00\u7ec4\u5de5\u51b5\u3002")
        : uiText(u"\u5df2\u7f16\u8f91 %1 \u7ec4\u5de5\u51b5\u3002").arg(batchCalculationTable_->rowCount()));
}

void MainWindow::submitBatchCalculation()
{
    if (!batchCalculationTable_ || batchCalculationTable_->rowCount() == 0) {
        batchGenerationStatus_->setText(uiText(u"\u6279\u5904\u7406\u8ba1\u5212\u4e3a\u7a7a\uff0c\u8bf7\u5148\u70b9\u51fb + \u6dfb\u52a0\u5de5\u51b5\u3002"));
        return;
    }
    if (!workflow_ || workflow_->thread() != thread()) {
        batchGenerationStatus_->setText(uiText(u"\u5de5\u4f5c\u6d41\u670d\u52a1\u5c1a\u672a\u8fde\u63a5\uff0c\u672a\u63d0\u4ea4\u6279\u5904\u7406\u8ba1\u5212\u3002"));
        return;
    }

    const QString planId = QStringLiteral("ui-batch-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    const quint64 version = ++batchPlanVersion_;
    int accepted = 0;
    for (int row = 0; row < batchCalculationTable_->rowCount(); ++row) {
        RunRequest request;
        request.scope.projectId = QStringLiteral("mock-project");
        request.scope.caseId = batchCalculationTable_->item(row, 0)->text();
        request.batchPlanId = planId;
        request.batchPlanVersion = version;
        request.inputSnapshot.snapshotId = QStringLiteral("%1-row-%2-m%3-a%4-t%5")
            .arg(planId).arg(row + 1)
            .arg(batchCalculationTable_->item(row, 1)->text())
            .arg(batchCalculationTable_->item(row, 2)->text())
            .arg(batchCalculationTable_->item(row, 3)->text());
        request.inputSnapshot.scope = request.scope;
        request.inputSnapshot.revision = 1;
        const auto result = workflow_->requestRun(request);
        if (result.allowed) ++accepted;
        for (const auto &issue : result.issues)
            appendLog(QStringLiteral("ERROR"), QStringLiteral("Validation"), issue.message, planId);
    }
    batchGenerationStatus_->setText(uiText(u"\u5df2\u751f\u6210\u6279\u5904\u7406\u8ba1\u5212 %1\uff08v%2\uff09\uff0c\u63d0\u4ea4 %3 \u4e2a Mock \u6267\u884c\u8bf7\u6c42\u3002\u771f\u5b9e\u6c42\u89e3\u5668\u4ecd\u9700\u8fd0\u884c\u6a21\u5757\u63a5\u5165\u3002").arg(planId).arg(version).arg(accepted));
    if (batchRunProgress_ && batchRunSummary_) {
        batchRunProgress_->setRange(0, qMax(1, accepted));
        batchRunProgress_->setValue(0);
        batchRunProgress_->setFormat(uiText(u"Mock \u6279\u5904\u7406\uff1a0 / %m \u5df2\u63d0\u4ea4"));
        batchRunSummary_->setText(uiText(u"\u6279\u5904\u7406\u8ba1\u5212 %1    \u603b\u4efb\u52a1 %2    \u5df2\u63d0\u4ea4 %3    \u8fd0\u884c\u6a21\u5f0f Mock").arg(planId).arg(batchCalculationTable_->rowCount()).arg(accepted));
    }
    appendMessage(uiText(u"\u6279\u5904\u7406\u8ba1\u5212\u5df2\u63d0\u4ea4\uff1a%1 \u4e2a\u5b50\u8bf7\u6c42\uff08Mock\uff09").arg(accepted));
}

void MainWindow::refreshStateModel()
{
    for (int index = 0; index < stateIds_.size(); ++index) {
        const QString &id = stateIds_.at(index);
        if (stateDrafts_.contains(id)) continue;
        StateDraft draft;
        // Give the built-in examples distinct values so selecting a state
        // visibly proves that the calculation form follows the state model.
        if (index == 1) {
            draft.mach = 1.2;
            draft.angleOfAttack = 3.5;
            draft.temperature = 300.0;
            draft.altitude = 8000.0;
        } else if (index == 2) {
            draft.mach = 0.6;
            draft.angleOfAttack = -1.0;
            draft.temperature = 280.0;
            draft.altitude = 2000.0;
        }
        stateDrafts_.insert(id, draft);
    }
    if (stateTable_) {
        const QSignalBlocker blocker(stateTable_);
        stateTable_->setRowCount(stateIds_.size());
        for (int row = 0; row < stateIds_.size(); ++row) {
            const auto draft = stateDrafts_.value(stateIds_.at(row));
            stateTable_->setItem(row, 0, new QTableWidgetItem(stateIds_.at(row)));
            stateTable_->setItem(row, 1, new QTableWidgetItem(
                uiText(u"\u9a6c\u8d6b\u6570 %1\uff0c\u653b\u89d2 %2\uff0c\u6e29\u5ea6 %3 K\uff0c\u9ad8\u5ea6 %4 m")
                    .arg(draft.mach).arg(draft.angleOfAttack).arg(draft.temperature).arg(draft.altitude)));
            stateTable_->setItem(row, 2, new QTableWidgetItem(uiText(u"\u754c\u9762 Mock\uff1a\u5df2\u914d\u7f6e")));
        }
    }
    if (statesNavigationItem_) {
        qDeleteAll(statesNavigationItem_->takeChildren());
        for (const QString &id : stateIds_)
            addNavigationItem(statesNavigationItem_, id + uiText(u"\uff08Mock \u754c\u9762\u793a\u4f8b\uff09"), QStringLiteral("state"), id, QStringLiteral("state"));
        statesNavigationItem_->setExpanded(true);
    }
    if (singleCaseCombo_) {
        const QString current = singleCaseCombo_->currentText();
        const QSignalBlocker blocker(singleCaseCombo_);
        singleCaseCombo_->clear();
        singleCaseCombo_->addItems(stateIds_);
        const int index = singleCaseCombo_->findText(current);
        singleCaseCombo_->setCurrentIndex(index >= 0 ? index : 0);
        applyStateToSingleCalculation(singleCaseCombo_->currentText());
    }
}

void MainWindow::applyStateToSingleCalculation(const QString &stateId)
{
    if (!singleMachSpin_ || stateId.isEmpty()) return;
    const auto draft = stateDrafts_.value(stateId);
    {
        const QSignalBlocker machBlocker(singleMachSpin_);
        const QSignalBlocker aoaBlocker(singleAoASpin_);
        const QSignalBlocker temperatureBlocker(singleTemperatureSpin_);
        singleMachSpin_->setValue(draft.mach);
        singleAoASpin_->setValue(draft.angleOfAttack);
        singleTemperatureSpin_->setValue(draft.temperature);
    }
    if (singleParametersSummary_)
        singleParametersSummary_->setText(uiText(u"\u5f53\u524d\u5355\u6b21\u5de5\u51b5\uff1a%1\n\u9a6c\u8d6b\u6570 %2\uff0c\u653b\u89d2 %3\uff0c\u6e29\u5ea6 %4 K\uff0c\u9ad8\u5ea6 %5 m\uff0cMPI %6")
            .arg(stateId).arg(draft.mach).arg(draft.angleOfAttack)
            .arg(draft.temperature).arg(draft.altitude).arg(singleMpiSpin_->value()));
}

void MainWindow::addState()
{
    QDialog dialog(stateListPage_);
    dialog.setWindowTitle(uiText(u"\u65b0\u5efa\u5de5\u51b5"));
    auto *form = new QFormLayout(&dialog);
    auto *name = new QLineEdit(&dialog);
    name->setText(QStringLiteral("State_%1").arg(stateIds_.size() + 1, 3, 10, QLatin1Char('0')));
    auto *mach = new QDoubleSpinBox(&dialog);
    mach->setRange(0.01, 20.0); mach->setSingleStep(0.05); mach->setValue(0.8);
    auto *aoa = new QDoubleSpinBox(&dialog);
    aoa->setRange(-90.0, 90.0); aoa->setSingleStep(0.5); aoa->setValue(2.0);
    auto *temperature = new QDoubleSpinBox(&dialog);
    temperature->setRange(100.0, 2000.0); temperature->setValue(288.15); temperature->setSuffix(QStringLiteral(" K"));
    auto *altitude = new QDoubleSpinBox(&dialog);
    altitude->setRange(-1000.0, 100000.0); altitude->setValue(5000.0); altitude->setSuffix(QStringLiteral(" m"));
    form->addRow(uiText(u"\u5de5\u51b5 ID"), name);
    form->addRow(uiText(u"\u9a6c\u8d6b\u6570"), mach);
    form->addRow(uiText(u"\u653b\u89d2"), aoa);
    form->addRow(uiText(u"\u6e29\u5ea6"), temperature);
    form->addRow(uiText(u"\u9ad8\u5ea6"), altitude);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    const QString id = name->text().trimmed();
    if (id.isEmpty() || stateIds_.contains(id)) {
        appendLog(QStringLiteral("WARN"), QStringLiteral("State"), uiText(u"\u5de5\u51b5 ID \u4e3a\u7a7a\u6216\u5df2\u5b58\u5728\uff0c\u672a\u6dfb\u52a0\u3002"));
        return;
    }
    stateIds_.append(id);
    StateDraft draft;
    draft.mach = mach->value();
    draft.angleOfAttack = aoa->value();
    draft.temperature = temperature->value();
    draft.altitude = altitude->value();
    stateDrafts_.insert(id, draft);
    refreshStateModel();
    appendMessage(uiText(u"\u5df2\u6dfb\u52a0\u5de5\u51b5 %1\uff08\u754c\u9762 Mock\uff09").arg(id));
}

void MainWindow::editState()
{
    if (!stateTable_ || stateTable_->currentRow() < 0) return;
    const QString id = stateIds_.value(stateTable_->currentRow());
    StateDraft draft = stateDrafts_.value(id);
    QDialog dialog(stateListPage_);
    dialog.setWindowTitle(uiText(u"\u7f16\u8f91\u5de5\u51b5\u53c2\u6570"));
    auto *form = new QFormLayout(&dialog);
    form->addRow(uiText(u"\u5de5\u51b5 ID"), new QLabel(id, &dialog));
    auto *mach = new QDoubleSpinBox(&dialog);
    mach->setRange(0.01, 20.0); mach->setSingleStep(0.05); mach->setValue(draft.mach);
    auto *aoa = new QDoubleSpinBox(&dialog);
    aoa->setRange(-90.0, 90.0); aoa->setSingleStep(0.5); aoa->setValue(draft.angleOfAttack);
    auto *temperature = new QDoubleSpinBox(&dialog);
    temperature->setRange(100.0, 2000.0); temperature->setValue(draft.temperature); temperature->setSuffix(QStringLiteral(" K"));
    auto *altitude = new QDoubleSpinBox(&dialog);
    altitude->setRange(-1000.0, 100000.0); altitude->setValue(draft.altitude); altitude->setSuffix(QStringLiteral(" m"));
    form->addRow(uiText(u"\u9a6c\u8d6b\u6570"), mach);
    form->addRow(uiText(u"\u653b\u89d2"), aoa);
    form->addRow(uiText(u"\u6e29\u5ea6"), temperature);
    form->addRow(uiText(u"\u9ad8\u5ea6"), altitude);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    draft.mach = mach->value();
    draft.angleOfAttack = aoa->value();
    draft.temperature = temperature->value();
    draft.altitude = altitude->value();
    stateDrafts_.insert(id, draft);
    refreshStateModel();
    appendMessage(uiText(u"\u5df2\u66f4\u65b0\u5de5\u51b5 %1 \u7684\u53c2\u6570\uff08\u754c\u9762 Mock\uff09").arg(id));
}

void MainWindow::removeState()
{
    if (!stateTable_ || stateTable_->currentRow() < 0) return;
    if (stateIds_.size() <= 1) {
        appendLog(QStringLiteral("WARN"), QStringLiteral("State"), uiText(u"\u81f3\u5c11\u4fdd\u7559\u4e00\u4e2a\u5de5\u51b5\u3002"));
        return;
    }
    const int row = stateTable_->currentRow();
    const QString id = stateIds_.at(row);
    stateIds_.removeAt(row);
    stateDrafts_.remove(id);
    refreshStateModel();
    appendMessage(uiText(u"\u5df2\u5220\u9664\u5de5\u51b5 %1\uff08\u754c\u9762 Mock\uff09").arg(id));
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
    QTreeWidgetItem *batchRuns = nullptr;
    QTreeWidgetItem *batchRun = nullptr;
    const QString batchId = tasks_ ? tasks_->record(event.taskId).batchId : QString();
    for (QTreeWidgetItemIterator it(projectTree_); *it; ++it) {
        const QString objectId = (*it)->data(0, ObjectIdRole).toString();
        if (objectId == event.taskId) navigationTask = *it;
        if (objectId == QStringLiteral("single-runs")) singleRuns = *it;
        if (objectId == QStringLiteral("batch-runs")) batchRuns = *it;
        if (!batchId.isEmpty() && objectId == batchId
            && (*it)->data(0, NodeTypeRole).toString() == QStringLiteral("batch-run")) batchRun = *it;
    }
    if (!batchId.isEmpty() && !batchRun && batchRuns) {
        batchRun = addNavigationItem(batchRuns, batchId + uiText(u"\uff08Mock \u6279\u5904\u7406\uff09"),
                                     QStringLiteral("batch-run"), batchId, QStringLiteral("batch-run"));
        batchRun->setExpanded(true);
    }
    if (!navigationTask && singleRuns && !event.taskId.isEmpty()) {
        auto *parent = batchRun ? batchRun : singleRuns;
        navigationTask = addNavigationItem(parent, event.taskId + uiText(u"\uff08Mock \u4e8b\u4ef6\uff09"),
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
    // Feed the live monitoring page as a second consumer. Its task set is its
    // own, so this adds a listener without changing what the task tree and log
    // table above already show for the same event.
    if (monitorPage_) {
        // The page rejects events whose attempt does not match the attempt it
        // holds, which is what keeps a stale delivery from attaching itself to
        // a newer run. A retry therefore has to reach it as a record first:
        // without this the service would move to a second attempt and every
        // event from it would be dropped as stale, leaving the page on the dead
        // attempt with no visible sign that anything moved on.
        if (tasks_) {
            const TaskRecord known = tasks_->record(event.taskId);
            if (!known.taskId.isEmpty() && monitorPage_->taskRecord(event.taskId).attemptId != known.attemptId)
                monitorPage_->upsertTask(known);
        }
        monitorPage_->displayTaskEvent(event);
    }
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
        : scope == QStringLiteral("current-project") ? uiText(u"\u5f53\u524d\u5de5\u4f5c\u533a") : scope;
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

void MainWindow::showTaskMonitor(const QString &taskId)
{
    if (taskId.isEmpty() || !taskLogView_) return;
    if (!currentTaskId_.isEmpty())
        taskLogScrollPositions_.insert(currentTaskId_, taskLogView_->verticalScrollBar()->value());
    currentTaskId_ = taskId;

    QString status = uiText(u"\u8fd0\u884c\u4e2d\uff08Mock\uff09");
    QString details = uiText(u"\u5f53\u524d\u8fed\u4ee3\uff1a1250    \u603b\u8fed\u4ee3\uff1a10000    \u6b8b\u5dee\uff1a1.2e-4\uff08Mock\uff09");
    int progressValue = 125;
    QString progressText = uiText(u"12.5%\uff08Mock\uff09");
    QString resources = uiText(u"CPU 76%    \u5185\u5b58 18.3 GB    MPI 32    \u8ba1\u7b97\u65f6\u95f4 00:12:35    \uff08Mock\uff09");
    if (taskId == QStringLiteral("Task_001")) {
        status = uiText(u"\u5df2\u5b8c\u6210\uff08Mock\uff09");
        details = uiText(u"\u5f53\u524d\u8fed\u4ee3\uff1a10000    \u603b\u8fed\u4ee3\uff1a10000    \u6b8b\u5dee\uff1a8.0e-7\uff08Mock\uff09");
        progressValue = 1000;
        progressText = uiText(u"100%\uff08Mock\uff09");
        resources = uiText(u"CPU 0%    \u5185\u5b58 0 GB    MPI 32    \u8ba1\u7b97\u65f6\u95f4 00:08:21    \uff08Mock\uff09");
    } else if (taskId == QStringLiteral("Task_002")) {
        details = uiText(u"\u5f53\u524d\u8fed\u4ee3\uff1a4200    \u603b\u8fed\u4ee3\uff1a10000    \u6b8b\u5dee\uff1a4.2e-3\uff08Mock\uff09");
        progressValue = 420;
        progressText = uiText(u"42%\uff08Mock\uff09");
        resources = uiText(u"CPU 81%    \u5185\u5b58 20.1 GB    MPI 32    \u8ba1\u7b97\u65f6\u95f4 00:04:10    \uff08Mock\uff09");
    } else if (taskId == QStringLiteral("Task_003")) {
        status = uiText(u"\u51c6\u5907\u4e2d\uff08Mock\uff09");
        details = uiText(u"\u6682\u65e0\u8fed\u4ee3\u4e0e\u6b8b\u5dee\u6570\u636e\uff08Mock\uff09");
        progressValue = 0;
        progressText = uiText(u"0%\uff08Mock\uff09");
        resources = uiText(u"CPU --    \u5185\u5b58 --    MPI 32    \u8ba1\u7b97\u65f6\u95f4 --    \uff08Mock\uff09");
    }
    taskMonitorTitle_->setText(taskId + uiText(u" \u00b7 \u7efc\u5408\u76d1\u63a7\uff08Mock\uff09"));
    monitorStatus_->setText(status);
    monitorDetails_->setText(details);
    progress_->setValue(progressValue);
    progress_->setFormat(progressText);
    taskCurveState_->setText(taskId + uiText(u" \u00b7 Mock \u66f2\u7ebf\u533a\u57df\n\u7b49\u5f85\u771f\u5b9e\u6b8b\u5dee\u6570\u636e\u4e0e\u66f2\u7ebf\u7ec4\u4ef6\u63a5\u5165"));
    taskResourceSummary_->setText(resources);
    refreshTaskLog();
    taskLogView_->verticalScrollBar()->setValue(
        taskLogScrollPositions_.value(taskId, taskLogView_->verticalScrollBar()->maximum()));
}

void MainWindow::refreshTaskLog()
{
    if (!taskLogView_ || !taskLogStreamFilter_) return;
    const QString filter = taskLogStreamFilter_->currentData().toString();
    taskLogView_->clear();
    QTextCursor cursor(taskLogView_->document());
    const auto entries = taskLogEntries_.value(currentTaskId_);
    for (const auto &entry : entries) {
        if (filter != QStringLiteral("all") && filter != entry.first) continue;
        QTextCharFormat format;
        if (entry.first == QStringLiteral("stderr")) format.setForeground(QColor(196, 83, 0));
        else format.setForeground(taskLogView_->palette().color(QPalette::Text));
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(QStringLiteral("[%1] %2\n").arg(entry.first, entry.second), format);
    }
    if (taskLogAutoScroll_->isChecked())
        taskLogView_->verticalScrollBar()->setValue(taskLogView_->verticalScrollBar()->maximum());
}

void MainWindow::appendTaskLogMock(const QString &stream, const QString &message)
{
    taskLogEntries_[currentTaskId_].append(qMakePair(stream, message));
    const QString filter = taskLogStreamFilter_->currentData().toString();
    if (filter != QStringLiteral("all") && filter != stream) return;
    auto *scrollBar = taskLogView_->verticalScrollBar();
    const bool wasAtBottom = scrollBar->value() >= scrollBar->maximum() - 2;
    QTextCursor cursor(taskLogView_->document());
    QTextCharFormat format;
    format.setForeground(stream == QStringLiteral("stderr")
        ? QColor(196, 83, 0) : taskLogView_->palette().color(QPalette::Text));
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("[%1] %2\n").arg(stream, message), format);
    if (taskLogAutoScroll_->isChecked() && wasAtBottom)
        scrollBar->setValue(scrollBar->maximum());
}

void MainWindow::updateTaskProgressMock(const QString &message)
{
    auto &entries = taskLogEntries_[currentTaskId_];
    int progressIndex = -1;
    for (int index = entries.size() - 1; index >= 0; --index) {
        if (entries[index].second.startsWith(QStringLiteral("Progress:"))) {
            progressIndex = index;
            break;
        }
    }
    if (progressIndex < 0) {
        appendTaskLogMock(QStringLiteral("stdout"), message);
        return;
    }
    entries[progressIndex].second = message;
    const QString filter = taskLogStreamFilter_->currentData().toString();
    if (filter != QStringLiteral("all") && filter != QStringLiteral("stdout")) return;
    QTextCursor cursor(taskLogView_->document());
    cursor.movePosition(QTextCursor::End);
    cursor.movePosition(QTextCursor::PreviousBlock);
    const QString progressPrefix = QStringLiteral("[stdout] Progress:");
    if (!cursor.block().text().startsWith(progressPrefix)) {
        refreshTaskLog();
        return;
    }
    cursor.select(QTextCursor::BlockUnderCursor);
    QTextCharFormat format;
    format.setForeground(taskLogView_->palette().color(QPalette::Text));
    cursor.insertText(QStringLiteral("[stdout] %1").arg(message), format);
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
