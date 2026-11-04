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
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QDockWidget>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTabWidget>
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
    if (!check(window.findChild<QDockWidget *>("projectDock")->windowTitle() == text(u"\u5de5\u4f5c\u533a\u6811"),
               "Dock title has incorrect Unicode characters")) return 13;
    if (!check(window.findChild<QTreeWidget *>("workspaceTree")->topLevelItem(0)->child(0)->text(0)
               == text(u"\u524d\u5904\u7406"), "Workspace tree label has incorrect Unicode characters")) return 14;
    bool hasNewProject = false;
    for (auto *action : window.findChildren<QAction *>())
        hasNewProject = hasNewProject || action->text() == text(u"\u65b0\u5efa\u5de5\u4f5c\u533a");
    if (!check(hasNewProject, "Workspace Ribbon action has incorrect Unicode characters")) return 15;
    if (!check(window.findChild<QLabel *>("propertyTitle")->text() == text(u"\u7f51\u683c\u6587\u4ef6"),
               "Property title has incorrect Unicode characters")) return 16;
    if (!check(window.findChild<QTreeWidget *>("workspaceTree")->topLevelItemCount() == 1, "Missing workspace navigation")) return 1;
    const auto categories = window.ribbonBar()->categoryPages();
    const QStringList expectedCategories = {QStringLiteral("preprocessCategory"), QStringLiteral("parametersCategory"),
        QStringLiteral("calculationCategory"), QStringLiteral("intelligentDataCategory"),
        QStringLiteral("postprocessAnalysisCategory")};
    bool categoriesMatch = categories.size() == expectedCategories.size();
    for (int index = 0; categoriesMatch && index < categories.size(); ++index)
        categoriesMatch = categories[index]->objectName() == expectedCategories[index];
    if (!check(categoriesMatch, "Expected the five specified functional Ribbon tabs")) return 17;
    auto *preprocess = categories[0];
    auto *workspace = window.findChild<QStackedWidget *>("centralWorkspace");
    auto *projectTree = window.findChild<QTreeWidget *>("workspaceTree");
    auto *root = projectTree ? projectTree->topLevelItem(0) : nullptr;
    if (!check(workspace && projectTree && root && root->childCount() == 5
               && !window.findChild<QStackedWidget *>("navigationStack")
               && !window.findChild<QTreeWidget *>("parameterTree"),
               "Workspace navigation must use one unified tree")) return 32;
    auto *preprocessNode = root->child(0);
    auto *parameterNode = root->child(1);
    auto *calculationNode = root->child(2);
    auto *intelligentDataNode = root->child(3);
    auto *postprocessNode = root->child(4);
    if (!check(preprocessNode->childCount() == 2 && parameterNode->childCount() == 7
               && calculationNode->childCount() == 2
               && intelligentDataNode->childCount() == 0 && postprocessNode->childCount() == 0,
               "Unified tree module hierarchy is incorrect")) return 54;
    if (!check(window.findChild<QTreeWidget *>("workflowTree")->isHidden(),
               "Workflow tree must not occupy the module navigation")) return 33;
    projectTree->setCurrentItem(preprocessNode->child(0)->child(2));
    projectTree->setCurrentItem(preprocessNode->child(0)->child(1));
    app.processEvents();
    auto *retainedPage = workspace->currentWidget();
    parameterNode->setExpanded(false);
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("parametersCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == parameterNode && parameterNode->isExpanded()
               && workspace->currentWidget() == retainedPage,
               "Ribbon must locate the module without replacing the main display")) return 34;
    if (!check(parameterNode->childCount() == 7 && parameterNode->child(0)->childCount() == 0,
               "Parameter tree must contain seven leaf pages")) return 41;
    projectTree->setCurrentItem(parameterNode->child(0));
    app.processEvents();
    auto *parameterDialog = window.findChild<QDialog *>("parameterDialog_control-equations");
    if (!check(parameterDialog && parameterDialog->isVisible()
               && window.ribbonBar()->currentIndex() == window.ribbonBar()->categoryIndex(
                   window.findChild<SARibbonCategory *>("parametersCategory")),
               "Parameter node did not open its dialog or reverse-sync Ribbon")) return 52;
    parameterDialog->close();
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("calculationCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == calculationNode && calculationNode->isExpanded()
               && parameterNode->isExpanded(), "Ribbon location collapsed another module")) return 55;
    auto *rememberedMonitorItem = calculationNode->child(1)->child(0)->child(0);
    projectTree->setCurrentItem(rememberedMonitorItem);
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("intelligentDataCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == intelligentDataNode
               && workspace->currentWidget()->objectName() == "taskMonitorPage",
               "Reserved data entry must not replace the current page")) return 53;
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("postprocessAnalysisCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == postprocessNode, "Postprocess Ribbon did not locate its node")) return 36;
    window.ribbonBar()->raiseCategory(preprocess);
    app.processEvents();
    if (!check(projectTree->currentItem() == preprocessNode
               && workspace->currentWidget()->objectName() == "taskMonitorPage",
               "Preprocess Ribbon tab must retain the persistent main display")) return 51;
    window.show();
    app.processEvents();
    auto *projectDock = window.findChild<QDockWidget *>("projectDock");
    auto *propertyDock = window.findChild<QDockWidget *>("propertyDock");
    if (!check(window.dockWidgetArea(projectDock) == Qt::LeftDockWidgetArea
               && window.dockWidgetArea(propertyDock) == Qt::LeftDockWidgetArea
               && projectDock->geometry().left() < propertyDock->geometry().left()
               && projectDock->minimumWidth() >= 220 && propertyDock->minimumWidth() >= 300,
               "Default layout must place navigation before properties on the left")) return 42;
    auto *outputDock = window.findChild<QDockWidget *>("outputDock");
    auto *outputTable = window.findChild<QTableWidget *>("outputLogTable");
    auto *levelFilter = window.findChild<QComboBox *>("logLevelFilter");
    auto *moduleFilter = window.findChild<QComboBox *>("logModuleFilter");
    auto *logSearch = window.findChild<QLineEdit *>("logSearch");
    if (!check(outputDock && outputDock->windowTitle() == text(u"\u8f93\u51fa")
               && outputTable && outputTable->columnCount() == 4
               && !outputDock->findChild<QTabWidget *>()
               && levelFilter && moduleFilter && logSearch
               && window.findChild<QCheckBox *>("logAutoScroll")->isChecked()
               && !window.findChild<QPushButton *>("exportOutputButton")->isEnabled(),
               "Bottom area must be a single structured Output panel")) return 44;
    window.findChild<QPushButton *>("outputScopeReset")->click();
    levelFilter->setCurrentIndex(levelFilter->findData("WARN"));
    moduleFilter->setCurrentIndex(moduleFilter->findData("Validation"));
    logSearch->setText(text(u"\u5de5\u51b5"));
    app.processEvents();
    int visibleLogs = 0;
    for (int row = 0; row < outputTable->rowCount(); ++row) visibleLogs += !outputTable->isRowHidden(row);
    if (!check(visibleLogs == 1, "Output filters and search did not combine correctly")) return 45;
    levelFilter->setCurrentIndex(levelFilter->findData("ALL"));
    moduleFilter->setCurrentIndex(moduleFilter->findData("ALL"));
    logSearch->clear();
    auto *collapseOutput = window.findChild<QPushButton *>("outputCollapseButton");
    collapseOutput->click();
    app.processEvents();
    if (!check(window.findChild<QWidget *>("outputBody")->isHidden()
               && collapseOutput->text() == text(u"\u5c55\u5f00"),
               "Output panel did not collapse to its toolbar")) return 46;
    collapseOutput->click();
    app.processEvents();
    if (!check(!window.findChild<QWidget *>("outputBody")->isHidden(),
               "Output panel did not restore its content")) return 47;
    if (!check(preprocess->panelByObjectName("workspaceGroup")
               && preprocess->panelByObjectName("meshManagementGroup")
               && preprocess->panelByObjectName("trajectoryGroup")
               && preprocess->panelByObjectName("viewGroup"), "Missing grouped Preprocess commands")) return 18;
    if (!check(window.findChild<QAction *>("showStartupAction")
               && window.findChild<QAction *>("showStartupAction")->isEnabled(),
               "Workbench must expose a return-to-startup entry")) return 56;
    auto *postprocessCategory = window.findChild<SARibbonCategory *>("postprocessAnalysisCategory");
    if (!check(postprocessCategory && postprocessCategory->panelByObjectName("toolsGroup"),
               "Existing UI tools must remain reachable")) return 19;
    auto *newProjectMenu = window.findChild<QMenu *>("newProjectMenu");
    auto *displayModeMenu = window.findChild<QMenu *>("displayModeMenu");
    if (!check(newProjectMenu && newProjectMenu->actions().size() == 3
               && displayModeMenu && displayModeMenu->actions().size() == 2,
               "Missing Word-style Ribbon drop-down menus")) return 30;
    if (!check(!window.findChild<QAction *>("blankProjectMenuAction")->isEnabled()
               && !window.findChild<QAction *>("surfaceModeMenuAction")->isEnabled(),
               "Drop-down menu availability does not match service boundaries")) return 31;
    auto *plansItem = calculationNode->child(0)->child(1);
    projectTree->setCurrentItem(plansItem);
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "batchPlanPage"
               && projectTree->currentItem()->data(0, Qt::UserRole + 1).toString() == "batch-plans",
               "Batch configuration did not locate and open BatchPlan management")) return 38;
    if (!check(window.findChild<QTableWidget *>("batchPlanTable")->columnCount() == 3,
               "BatchPlan page is missing its model-neutral table")) return 39;
    auto *batchRunItem = calculationNode->child(1)->child(1)->child(0);
    auto *batchTaskItem = batchRunItem->child(0);
    projectTree->setCurrentItem(batchRunItem);
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "batchMonitorPage"
               && window.findChild<QLabel *>("outputTitle")->text().contains("BatchRun_001")
               && window.findChild<QTreeWidget *>("taskTree")->topLevelItemCount() == 3,
               "BatchRun selection did not open the integrated batch monitor")) return 48;
    projectTree->setCurrentItem(batchTaskItem);
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "taskMonitorPage"
               && window.findChild<QLabel *>("taskMonitorTitle")->text().contains("Task_001")
               && window.findChild<QWidget *>("convergenceCurvePanel")
               && window.findChild<QLabel *>("taskResourceSummary")
               && window.findChild<QLabel *>("outputTitle")->text().contains("Task_001"),
               "Task selection did not open the integrated monitor and scoped output")) return 49;
    for (auto *action : window.findChildren<QAction *>()) {
        if (action->property("ribbonGroup").isValid()
            && !check(!action->icon().pixmap(32, 32).isNull(), "Missing embedded Ribbon icon")) return 20;
    }
    projectTree->setCurrentItem(preprocessNode->child(0)->child(2));
    projectTree->setCurrentItem(preprocessNode->child(0)->child(1));
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "mesh-transformPage",
               "Mesh transform node did not open its placeholder page")) return 43;
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
    QTreeWidgetItem *taskRow = nullptr;
    auto *taskTree = window.findChild<QTreeWidget *>("taskTree");
    for (int row = 0; row < taskTree->topLevelItemCount(); ++row)
        if (taskTree->topLevelItem(row)->text(0) == "test-task") taskRow = taskTree->topLevelItem(row);
    if (!check(taskRow && taskTree->topLevelItemCount() == 4, "Task rows not updated by id")) return 7;
    QTreeWidgetItem *navigationTask = nullptr;
    for (QTreeWidgetItemIterator it(projectTree); *it; ++it)
        if ((*it)->data(0, Qt::UserRole + 1).toString() == "test-task") navigationTask = *it;
    if (!check(navigationTask && navigationTask->data(0, Qt::UserRole + 3).toInt()
               == static_cast<int>(aerosim::TaskStatus::Finished),
               "Task event did not locally update structured monitor status")) return 40;
    window.findChild<QPushButton *>("clearOutputButton")->click();
    if (!check(outputTable->rowCount() == 0, "Clear Output must only clear the UI rows")) return 50;
    window.bindServices({});
    if (!check(!run->isEnabled(), "Unbinding must disable run")) return 8;
    QTextStream(stdout) << "UI ownership, service binding, workflow gating and task display passed\n";
    return 0;
}
