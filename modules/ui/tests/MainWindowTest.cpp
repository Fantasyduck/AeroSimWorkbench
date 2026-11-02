#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include <QAction>
#include <QSettings>
#include <QTemporaryDir>
#include <QDir>
#include <QPixmap>
#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QTextStream>

class Workflow final : public aerosim::IWorkflowService {
public:
    using IWorkflowService::IWorkflowService;
    aerosim::WorkflowView view(const aerosim::ScopeRef &scope) const override {
        aerosim::WorkflowView result;
        result.scope = scope;
        result.actions.append({QStringLiteral("run"), true, {}, {}, {}, {}, {}});
        return result;
    }
    aerosim::ValidationResult requestRun(const aerosim::RunRequest &) override { ++requests; return {true, {}}; }
    void onTaskEvent(const aerosim::TaskEvent &) override {}
    int requests = 0;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir preferences;
    if (!preferences.isValid()) return 22;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences.path());
    aerosim::MainWindow window;
    const auto check = [](bool passed, const char *message) {
        if (!passed) QTextStream(stderr) << message << '\n';
        return passed;
    };
    // ASCII source with explicit UTF-16 expectations detects compiler/code-page corruption.
    const auto text = [](const char16_t *value) {
        return QString::fromUtf16(reinterpret_cast<const ushort *>(value));
    };
    if (!check(window.windowTitle() == text(u"AeroSimWorkbench \u00b7 \u4eff\u771f\u5de5\u4f5c\u53f0\uff08Mock \u670d\u52a1\uff09"),
               "Window title has incorrect Unicode characters")) return 12;
    if (!check(window.findChild<QDockWidget *>("projectDock")->windowTitle() == text(u"\u6a21\u5757\u5bfc\u822a"),
               "Dock title has incorrect Unicode characters")) return 13;
    if (!check(window.findChild<QTreeWidget *>("projectTree")->topLevelItem(0)->child(0)->text(0)
               == text(u"\u7f51\u683c Mesh"), "Project tree label has incorrect Unicode characters")) return 14;
    bool hasNewProject = false;
    for (auto *action : window.findChildren<QAction *>())
        hasNewProject = hasNewProject || action->text() == text(u"\u65b0\u5efa\u5de5\u7a0b");
    if (!check(hasNewProject, "Ribbon action has incorrect Unicode characters")) return 15;
    if (!check(window.findChild<QLabel *>("propertyTitle")->text() == text(u"\u7f51\u683c\u4e0e\u53d8\u6362"),
               "Property title has incorrect Unicode characters")) return 16;
    if (!check(window.findChild<QTreeWidget *>("projectTree")->topLevelItemCount() == 1, "Missing project navigation")) return 1;
    const auto categories = window.ribbonBar()->categoryPages();
    if (!check(categories.size() == 5 && categories[0]->objectName() == "startCategory",
               "Expected Start, Monitor, Postprocess, Data and Help tabs")) return 17;
    auto *start = categories[0];
    auto *navigation = window.findChild<QStackedWidget *>("navigationStack");
    auto *workspace = window.findChild<QStackedWidget *>("centralWorkspace");
    auto *projectTree = window.findChild<QTreeWidget *>("projectTree");
    auto *monitorTree = window.findChild<QTreeWidget *>("monitorTree");
    auto *dataTree = window.findChild<QTreeWidget *>("dataProcessTree");
    if (!check(navigation && workspace && projectTree && monitorTree && dataTree
               && navigation->currentWidget() == projectTree,
               "Missing stacked module navigation")) return 32;
    if (!check(window.findChild<QTreeWidget *>("workflowTree")->isHidden(),
               "Workflow tree must not occupy the module navigation")) return 33;
    QWidget *workspaceBeforeTabSwitch = workspace->currentWidget();
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("monitorCategory"));
    app.processEvents();
    if (!check(navigation->currentWidget() == monitorTree && workspace->currentWidget() == workspaceBeforeTabSwitch,
               "Monitor Ribbon tab must change only the navigation tree")) return 34;
    auto *rememberedMonitorItem = monitorTree->topLevelItem(0)->child(0)->child(0)->child(0);
    monitorTree->setCurrentItem(rememberedMonitorItem);
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("dataCategory"));
    app.processEvents();
    if (!check(navigation->currentWidget() == dataTree, "Data Ribbon tab did not select the data tree")) return 35;
    auto *datasetItem = dataTree->topLevelItem(0)->child(0)->child(0);
    dataTree->setCurrentItem(datasetItem);
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "dataProcessPage"
               && window.findChild<QLabel *>("propertyTitle")->text().contains("Dataset_001"),
               "Dataset selection did not open data processing details")) return 41;
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("helpCategory"));
    app.processEvents();
    if (!check(navigation->currentWidget() == dataTree, "Help must retain the previous navigation tree")) return 36;
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("monitorCategory"));
    app.processEvents();
    if (!check(monitorTree->currentItem() == rememberedMonitorItem,
               "Navigation selection was not preserved across Ribbon tabs")) return 37;
    window.ribbonBar()->raiseCategory(start);
    app.processEvents();
    window.show();
    app.processEvents();
    auto *projectDock = window.findChild<QDockWidget *>("projectDock");
    auto *propertyDock = window.findChild<QDockWidget *>("propertyDock");
    if (!check(window.dockWidgetArea(projectDock) == Qt::LeftDockWidgetArea
               && window.dockWidgetArea(propertyDock) == Qt::LeftDockWidgetArea
               && projectDock->geometry().left() < propertyDock->geometry().left()
               && projectDock->minimumWidth() >= 220 && propertyDock->minimumWidth() >= 300,
               "Default layout must place navigation before properties on the left")) return 42;
    if (!check(start->panelCount() == 5 && start->panelByObjectName("projectGroup")
               && start->panelByObjectName("batchGroup") && start->panelByObjectName("preprocessGroup")
               && start->panelByObjectName("parametersGroup") && start->panelByObjectName("viewGroup"), "Missing grouped Start commands")) return 18;
    auto *help = window.findChild<SARibbonCategory *>("helpCategory");
    if (!check(help && help->panelByObjectName("toolsGroup"), "Tools must be under Help")) return 19;
    auto *newProjectMenu = window.findChild<QMenu *>("newProjectMenu");
    auto *batchMenu = window.findChild<QMenu *>("batchMenu");
    auto *displayModeMenu = window.findChild<QMenu *>("displayModeMenu");
    if (!check(newProjectMenu && newProjectMenu->actions().size() == 3
               && batchMenu && batchMenu->actions().size() == 3
               && displayModeMenu && displayModeMenu->actions().size() == 2,
               "Missing Word-style Ribbon drop-down menus")) return 30;
    if (!check(!window.findChild<QAction *>("blankProjectMenuAction")->isEnabled()
               && window.findChild<QAction *>("openBatchConfigurationAction")->isEnabled()
               && !window.findChild<QAction *>("surfaceModeMenuAction")->isEnabled(),
               "Drop-down menu availability does not match service boundaries")) return 31;
    window.findChild<QAction *>("openBatchConfigurationAction")->trigger();
    app.processEvents();
    if (!check(navigation->currentWidget() == projectTree
               && workspace->currentWidget()->objectName() == "batchPlanPage"
               && projectTree->currentItem()->data(0, Qt::UserRole + 1).toString() == "batch-plans",
               "Batch configuration did not locate and open BatchPlan management")) return 38;
    if (!check(window.findChild<QTableWidget *>("batchPlanTable")->columnCount() == 3,
               "BatchPlan page is missing its model-neutral table")) return 39;
    for (auto *action : window.findChildren<QAction *>()) {
        if (action->property("ribbonGroup").isValid()
            && !check(!action->icon().pixmap(32, 32).isNull(), "Missing embedded Ribbon icon")) return 20;
    }
    window.findChild<QAction *>("stateAction")->trigger();
    if (!check(window.findChild<QLabel *>("propertyTitle")->text().contains(text(u"\u4e00\u6b21\u8ba1\u7b97")),
               "State must represent one calculation")) return 21;
    window.findChild<QAction *>("transformAction")->trigger();
    if (!check(!window.findChild<QDoubleSpinBox *>("X")->isEnabled(),
               "Mock project must not expose editable project parameters")) return 43;
    app.processEvents();
    for (const QString &id : {QStringLiteral("office-blue"), QStringLiteral("office-light"),
                              QStringLiteral("dark"), QStringLiteral("dark-gray")}) {
        auto *action = window.findChild<QAction *>(QStringLiteral("theme-") + id);
        if (!check(action, "Missing theme action")) return 23;
        action->trigger();
        const QString qss = window.styleSheet();
        action->trigger();
        if (!check(action->isChecked() && window.property("windowThemeId").toString() == id
                   && window.styleSheet() == qss, "Theme switch must be checked and idempotent")) return 24;
        if (id == "office-blue"
            && !check(window.ribbonBar()->windowTitleBackgroundBrush().color() == QColor("#2b579a")
                      && window.ribbonBar()->windowTitleTextColor() == QColor(Qt::white)
                      && qss.contains("SARibbonTabBar::tab:selected"),
                      "Office blue theme must use Word-like blue title and tab chrome")) return 28;
        if (id != "office-blue"
            && !check(window.ribbonBar()->windowTitleBackgroundBrush().style() == Qt::NoBrush,
                      "Non-blue themes must clear the Word title background")) return 29;
        QSettings saved(QStringLiteral("AeroSimWorkbench"), QStringLiteral("Appearance"));
        if (!check(saved.value("theme").toString() == id, "Theme selection not saved")) return 25;
        const QString screenshots = qEnvironmentVariable("AEROSIM_THEME_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            window.show();
            app.processEvents();
            window.grab().save(QDir(screenshots).filePath(id + QStringLiteral(".png")));
        }
    }
    {
        aerosim::MainWindow reopened;
        app.processEvents();
        if (!check(reopened.property("windowThemeId").toString() == "dark-gray", "Saved theme not restored")) return 26;
    }
    {
        QSettings saved(QStringLiteral("AeroSimWorkbench"), QStringLiteral("Appearance"));
        saved.setValue("theme", "unknown-theme");
        aerosim::MainWindow fallback;
        app.processEvents();
        if (!check(fallback.property("windowThemeId").toString() == "office-blue", "Invalid theme must fall back")) return 27;
    }
    for (auto slot : {aerosim::WidgetSlot::Viewport, aerosim::WidgetSlot::Log, aerosim::WidgetSlot::Error, aerosim::WidgetSlot::Monitor}) {
        auto *widget = new QLabel("Injected component");
        if (!check(window.insertWidget(slot, widget) && window.isAncestorOf(widget), "Widget ownership not transferred")) return 2;
        auto *duplicate = new QLabel;
        bool rejected = !window.insertWidget(slot, duplicate) && !duplicate->parent();
        delete duplicate;
        if (!check(rejected, "Occupied slot must reject without taking ownership")) return 3;
    }
    aerosim::PostProcessWorkbenchWindow postprocess;
    QObject::connect(&window, &aerosim::MainWindow::openPostProcessRequested,
                     &postprocess, &QWidget::show);
    auto *openPostprocess = window.findChild<QAction *>("openPostProcessAction");
    if (!check(openPostprocess && openPostprocess->isEnabled(), "Missing postprocess entry")) return 9;
    openPostprocess->trigger();
    if (!check(postprocess.isVisible(), "Postprocess entry failed to open workbench")) return 10;
    postprocess.close();
    openPostprocess->trigger();
    if (!check(postprocess.isVisible(), "Closed workbench cannot be reopened")) return 11;
    postprocess.close();
    Workflow workflow;
    aerosim::ServiceBindings bindings;
    bindings.workflow = &workflow;
    window.bindServices(bindings);
    window.bindServices(bindings);
    aerosim::RunRequest request;
    request.scope.caseId = "case";
    request.inputSnapshot.snapshotId = "snapshot";
    window.setRunContext(request);
    auto *run = window.findChild<QAction *>("runAction");
    if (!check(run && run->isEnabled(), "Workflow action not reflected")) return 4;
    run->trigger();
    if (!check(workflow.requests == 1, "Repeated binding duplicated run requests")) return 5;
    auto view = workflow.view(request.scope);
    view.actions[0].enabled = false;
    window.displayWorkflow(view);
    if (!check(!run->isEnabled(), "Blocked action remains enabled")) return 6;
    aerosim::TaskEvent event;
    event.taskId = "test-task";
    event.status = aerosim::TaskStatus::Preparing;
    window.displayTaskEvent(event);
    event.status = aerosim::TaskStatus::Finished;
    window.displayTaskEvent(event);
    if (!check(window.findChild<QTreeWidget *>("taskTree")->topLevelItemCount() == 1, "Task rows not updated by id")) return 7;
    QTreeWidgetItem *navigationTask = nullptr;
    for (QTreeWidgetItemIterator it(monitorTree); *it; ++it)
        if ((*it)->data(0, Qt::UserRole + 1).toString() == "test-task") navigationTask = *it;
    if (!check(navigationTask && navigationTask->data(0, Qt::UserRole + 3).toInt()
               == static_cast<int>(aerosim::TaskStatus::Finished),
               "Task event did not locally update structured monitor status")) return 40;
    window.bindServices({});
    if (!check(!run->isEnabled(), "Unbinding must disable run")) return 8;
    QTextStream(stdout) << "UI ownership, service binding, workflow gating and task display passed\n";
    return 0;
}
