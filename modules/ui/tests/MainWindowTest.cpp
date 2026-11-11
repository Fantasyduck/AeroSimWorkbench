#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include <QAbstractButton>
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
#include <QListWidget>
#include <QMenu>
#include <QDockWidget>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QTextDocument>
#include <QTextStream>

class Workflow final : public aerosim::IWorkflowService {
public:
    using IWorkflowService::IWorkflowService;
    aerosim::AccessResult<aerosim::WorkflowViewV2> viewV2(const aerosim::ScopeRef &scope) const override {
        aerosim::WorkflowViewV2 result;
        result.scope = scope;
        result.actions.append({QStringLiteral("run"), true, {}, {}, {}, {}, {}});
        return aerosim::AccessResult<aerosim::WorkflowViewV2>::success(result);
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
    int startupRequests = 0;
    QObject::connect(&window, &aerosim::MainWindow::showStartupRequested,
                     &window, [&startupRequests] { ++startupRequests; });
    auto *returnToStartup = window.findChild<QAbstractButton *>("returnToStartupButton");
    if (!check(returnToStartup
               && returnToStartup == window.ribbonBar()->applicationButton()
               && returnToStartup->text() == text(u"\u8fd4\u56de\u6b22\u8fce\u9875"),
               "Ribbon application button must return to the startup page")) return 69;
    returnToStartup->click();
    if (!check(startupRequests == 1, "Startup button did not emit the existing navigation request")) return 70;
    if (!check(window.findChild<QLabel *>("propertyTitle")->text() == text(u"\u7f51\u683c\u7ba1\u7406"),
               "Property title has incorrect Unicode characters")) return 16;
    if (!check(window.findChild<QTreeWidget *>("workspaceTree")->topLevelItemCount() == 1, "Missing workspace navigation")) return 1;
    const auto categories = window.ribbonBar()->categoryPages();
    const QStringList expectedCategories = {QStringLiteral("preprocessCategory"), QStringLiteral("parametersCategory"),
        QStringLiteral("calculationCategory"), QStringLiteral("intelligentDataCategory"),
        QStringLiteral("postprocessAnalysisCategory"), QStringLiteral("helpCategory")};
    bool categoriesMatch = categories.size() == expectedCategories.size();
    for (int index = 0; categoriesMatch && index < categories.size(); ++index)
        categoriesMatch = categories[index]->objectName() == expectedCategories[index];
    if (!check(categoriesMatch, "Expected five functional Ribbon tabs followed by Help")) return 17;
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
    // 智能数据处理 carries the two result pages. They were checked as "no
    // children" while the module was an empty placeholder; the count is pinned
    // to the two entries the module actually has rather than to a number that
    // only happened to be zero.
    const bool intelligentChildrenOk =
        intelligentDataNode->childCount() == 2
        && intelligentDataNode->child(0)->data(0, Qt::UserRole).toString() == QStringLiteral("result-data-table")
        && intelligentDataNode->child(1)->data(0, Qt::UserRole).toString() == QStringLiteral("result-summary-table");
    if (!check(preprocessNode->childCount() == 2 && parameterNode->childCount() == 8
               && calculationNode->childCount() == 2
               && intelligentChildrenOk && postprocessNode->childCount() == 0,
               "Unified tree module hierarchy is incorrect")) return 54;
    auto *helpCategory = categories[5];
    auto *pageBeforeHelp = workspace->currentWidget();
    auto *treeItemBeforeHelp = projectTree->currentItem();
    window.ribbonBar()->raiseCategory(helpCategory);
    app.processEvents();
    if (!check(workspace->currentWidget() == pageBeforeHelp
               && projectTree->currentItem() == treeItemBeforeHelp
               && root->childCount() == 5,
               "Help Ribbon must not alter the business workspace or unified tree")) return 80;
    window.ribbonBar()->raiseCategory(preprocess);
    auto *meshPage = window.findChild<QWidget *>("viewportPage");
    auto *meshTable = window.findChild<QTableWidget *>("meshTable");
    auto *meshCount = window.findChild<QLabel *>("meshObjectCount");
    auto *importMesh = window.findChild<QPushButton *>("importMeshButton");
    auto *deleteMesh = window.findChild<QPushButton *>("deleteMeshButton");
    auto *scaleEditor = window.findChild<QDoubleSpinBox *>("meshScaleEditor");
    auto *historyTabs = window.findChild<QTabWidget *>("meshHistoryTabs");
    auto *history = window.findChild<QListWidget *>("meshOperationHistory");
    auto *historyCollapse = window.findChild<QPushButton *>("meshHistoryCollapseButton");
    auto *workspaceTabs = window.findChild<QTabWidget *>("meshWorkspaceTabs");
    if (!check(meshPage && meshTable && meshTable->rowCount() == 3
               && meshTable->columnCount() == 3
               && meshCount && meshCount->text().contains("3 / 10")
               && importMesh && deleteMesh && scaleEditor && historyTabs && history && historyCollapse
               && workspaceTabs && workspaceTabs->count() == 3
               && window.findChild<QSplitter *>("meshWorkspaceMainSplitter")
               && window.findChild<QSplitter *>("meshSettingsSplitter")
               && window.findChild<QWidget *>("viewportSlot")
               && window.findChild<QDockWidget *>("propertyDock")->isHidden()
               && window.findChild<QWidget *>("outputBody")->isHidden()
               && historyTabs->isHidden()
               && historyCollapse->text() == text(u"\u5c55\u5f00")
               && !window.findChild<QPlainTextEdit *>("meshScriptPlaceholder")->isEnabled(),
               "Mesh management workspace is incomplete")) return 57;
    // The replicated reference layout exposes a coordinate system selector and
    // a 序号/网格名称/是否可见 table next to the tabbed workspace.
    auto *solidRadio = window.findChild<QRadioButton *>("meshCoordinateSystemSolid");
    auto *planarRadio = window.findChild<QRadioButton *>("meshCoordinateSystemPlanar");
    if (!check(solidRadio && planarRadio && solidRadio->isChecked()
               && meshTable->horizontalHeaderItem(0)
               && meshTable->horizontalHeaderItem(0)->text() == text(u"\u5e8f\u53f7")
               && meshTable->horizontalHeaderItem(1)->text() == text(u"\u7f51\u683c\u540d\u79f0")
               && meshTable->horizontalHeaderItem(2)->text() == text(u"\u662f\u5426\u53ef\u89c1"),
               "Mesh information table does not match the reference layout")) return 86;
    // Initial visibility must mirror the seeded state (the first two meshes are
    // visible, the third is hidden). Assigning a checkable cell emits
    // itemChanged before its state is set, which used to be read back as the
    // user hiding every mesh.
    if (!check(meshTable->item(0, 2)->checkState() == Qt::Checked
               && meshTable->item(1, 2)->checkState() == Qt::Checked
               && meshTable->item(2, 2)->checkState() == Qt::Unchecked,
               "Initial mesh visibility was not preserved in the table")) return 87;
    if (!check(!history->count() || !history->item(history->count() - 1)->text()
                   .contains(text(u"\u9690\u85c5 Mesh_001")),
               "Seeding the table must not log a spurious hide")) return 88;
    meshTable->setCurrentCell(1, 1);
    scaleEditor->setValue(2.5);
    meshTable->item(1, 2)->setCheckState(Qt::Unchecked);
    meshTable->setCurrentCell(0, 1);
    meshTable->setCurrentCell(1, 1);
    app.processEvents();
    if (!check(scaleEditor->value() == 2.5
               && !window.findChild<QCheckBox *>("meshVisibleEditor")->isChecked()
               && history->count() >= 3,
               "Per-mesh mock display state was not retained")) return 58;
    for (int i = meshTable->rowCount(); i < 10; ++i) importMesh->click();
    if (!check(meshTable->rowCount() == 10 && !importMesh->isEnabled()
               && meshCount->text().contains("10 / 10"),
               "Mesh management did not enforce its ten-object UI limit")) return 59;
    deleteMesh->click();
    if (!check(meshTable->rowCount() == 9 && importMesh->isEnabled(),
               "Removing a mock mesh did not update list availability")) return 60;
    historyCollapse->click();
    app.processEvents();
    if (!check(!historyTabs->isHidden() && historyCollapse->text() == text(u"\u6536\u8d77"),
               "Mesh operation history did not expand")) return 61;
    historyCollapse->click();
    app.processEvents();
    if (!check(historyTabs->isHidden() && historyCollapse->text() == text(u"\u5c55\u5f00"),
               "Mesh operation history did not collapse")) return 62;
    if (!check(window.findChild<QTreeWidget *>("workflowTree")->isHidden(),
               "Workflow tree must not occupy the module navigation")) return 33;
    // Preprocess holds exactly two expand-free leaf nodes: mesh management and
    // the unified trajectory settings page.
    projectTree->setCurrentItem(preprocessNode->child(1));
    app.processEvents();
    auto *trajectoryPage = window.findChild<QWidget *>("trajectorySettingsPage");
    auto *selectTrajectoryFile = window.findChild<QPushButton *>("selectTrajectoryFileButton");
    if (!check(preprocessNode->childCount() == 2
               && preprocessNode->child(0)->childCount() == 0
               && preprocessNode->child(1)->childCount() == 0,
               "Preprocess navigation must expose two leaf nodes without sub-pages")) return 66;
    if (!check(trajectoryPage && workspace->currentWidget() == trajectoryPage
               && projectTree->currentItem() == preprocessNode->child(1)
               && selectTrajectoryFile && !selectTrajectoryFile->isEnabled()
               && window.findChild<QPushButton *>("loadTrajectoryDataButton")
               && window.findChild<QPushButton *>("extractTrajectoryStateButton")
               && window.findChild<QWidget *>("trajectoryFileSection")
               && !trajectoryPage->findChild<QWidget *>("viewportSlot"),
               "Trajectory leaf did not open the unified mock settings page")) return 66;
    projectTree->setCurrentItem(preprocessNode->child(0));
    app.processEvents();
    auto *retainedPage = workspace->currentWidget();
    if (!check(retainedPage->objectName() == "viewportPage",
               "Preprocess node did not establish its module page state")) return 66;
    parameterNode->setExpanded(false);
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("parametersCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == parameterNode && parameterNode->isExpanded()
               && workspace->currentWidget()->objectName() == "parameterWorkspacePage"
               && workspace->currentWidget() != retainedPage
               && !workspace->currentWidget()->findChild<QWidget *>("viewportSlot"),
               "Parameter Ribbon did not switch the central module page")) return 34;
    if (!check(parameterNode->childCount() == 8 && parameterNode->child(0)->childCount() == 0,
               "Parameter tree must contain eight leaf pages")) return 41;
    projectTree->setCurrentItem(parameterNode->child(0));
    app.processEvents();
    auto *parameterEditor = window.findChild<QWidget *>("parameterEditor_control-equations");
    if (!check(parameterEditor && !parameterEditor->isHidden()
               && workspace->currentWidget()->objectName() == "parameterEditor_control-equations"
               && !parameterEditor->isWindow()
               && window.findChild<QRadioButton *>("calculationType_steady")
               && window.findChild<QLabel *>("parameterWorkspaceTitle")->text().contains(
                    text(u"\u63a7\u5236\u65b9\u7a0b"))
               && window.ribbonBar()->currentIndex() == window.ribbonBar()->categoryIndex(
                   window.findChild<SARibbonCategory *>("parametersCategory")),
               "Parameter node did not embed its editor or reverse-sync Ribbon")) return 52;
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("calculationCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == calculationNode && calculationNode->isExpanded()
               && parameterNode->isExpanded()
               && workspace->currentWidget()->objectName() == "calculationOverviewPage",
               "Calculation Ribbon did not locate its tree node and overview")) return 55;
    auto *solverOutputMode = window.findChild<QComboBox *>("solverOutputModeCombo");
    auto *solverOutputDescription = window.findChild<QLabel *>("solverOutputModeDescription");
    if (!check(solverOutputMode && solverOutputMode->count() == 2
               && solverOutputMode->currentData().toString() == "embedded",
               "Calculation preparation is missing its default embedded output mode")) return 71;
    solverOutputMode->setCurrentIndex(1);
    if (!check(solverOutputDescription->text().contains(text(u"\u72ec\u7acb\u63a7\u5236\u53f0")),
               "Solver output mode did not retain its mock console configuration")) return 72;
    auto *rememberedMonitorItem = calculationNode->child(1)->child(0)->child(0);
    projectTree->setCurrentItem(rememberedMonitorItem);
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("intelligentDataCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == intelligentDataNode
               && workspace->currentWidget()->objectName() == "intelligentDataPage",
               "Intelligent data Ribbon did not show its reserved page")) return 53;

    // Both result pages are destinations of their own. The module entry and the
    // first page share one widget, so the inner stack index is the only thing
    // that distinguishes them -- and the only thing that can prove the summary
    // page is reachable at all rather than sitting behind a dead entry.
    auto *intelligentStack = window.findChild<QStackedWidget *>("intelligentDataStack");
    auto *resultDataPage = window.findChild<QWidget *>("resultDataTablePage");
    auto *resultSummaryPage = window.findChild<QWidget *>("resultSummaryTablePage");
    if (!check(intelligentStack && resultDataPage && resultSummaryPage
               && resultDataPage->parentWidget() == intelligentStack
               && resultSummaryPage->parentWidget() == intelligentStack,
               "Intelligent data module must host both result pages")) return 89;
    auto *resultDataAction = window.findChild<QAction *>("resultDataTableAction");
    auto *resultSummaryAction = window.findChild<QAction *>("resultSummaryTableAction");
    if (!check(resultDataAction && resultSummaryAction
               && resultDataAction->isEnabled() && resultSummaryAction->isEnabled(),
               "Intelligent data Ribbon must expose both working result actions")) return 92;
    resultSummaryAction->trigger();
    app.processEvents();
    if (!check(intelligentStack->currentWidget() == resultSummaryPage
               && projectTree->currentItem() == intelligentDataNode->child(1),
               "Summary Ribbon action must synchronize the page and navigation")) return 93;
    resultDataAction->trigger();
    app.processEvents();
    if (!check(intelligentStack->currentWidget() == resultDataPage
               && projectTree->currentItem() == intelligentDataNode->child(0),
               "Result Ribbon action must synchronize the page and navigation")) return 94;
    projectTree->setCurrentItem(intelligentDataNode->child(0));
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "intelligentDataPage"
               && intelligentStack->currentWidget() == resultDataPage,
               "Result data table entry did not select the data page")) return 90;
    projectTree->setCurrentItem(intelligentDataNode->child(1));
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "intelligentDataPage"
               && intelligentStack->currentWidget() == resultSummaryPage,
               "Result summary table entry did not select the summary page")) return 91;
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("calculationCategory"));
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "taskMonitorPage",
               "Calculation module did not restore its last monitor page")) return 67;
    window.ribbonBar()->raiseCategory(window.findChild<SARibbonCategory *>("postprocessAnalysisCategory"));
    app.processEvents();
    if (!check(projectTree->currentItem() == postprocessNode
               && workspace->currentWidget()->objectName() == "postprocessOverviewPage",
               "Postprocess Ribbon did not locate its node and reserved page")) return 36;
    window.ribbonBar()->raiseCategory(preprocess);
    app.processEvents();
    if (!check(projectTree->currentItem() == preprocessNode
               && workspace->currentWidget() == retainedPage
               && workspace->currentWidget()->objectName() == "viewportPage",
               "Preprocess Ribbon did not restore its last page")) return 51;
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (auto *category : categories) {
            window.ribbonBar()->raiseCategory(category);
            app.processEvents();
        }
    }
    window.ribbonBar()->raiseCategory(preprocess);
    app.processEvents();
    if (!check(workspace->currentWidget() == retainedPage
               && window.findChildren<QWidget *>("viewportSlot").size() == 1
               && meshTable->rowCount() == 9,
               "Repeated module switching recreated state or the VTK host")) return 68;
    window.show();
    app.processEvents();
    auto *projectDock = window.findChild<QDockWidget *>("projectDock");
    auto *propertyDock = window.findChild<QDockWidget *>("propertyDock");
    if (!check(window.dockWidgetArea(projectDock) == Qt::LeftDockWidgetArea
               && window.dockWidgetArea(propertyDock) == Qt::LeftDockWidgetArea
               && projectDock->geometry().left() < propertyDock->geometry().left()
               && !propertyDock->isFloating()
               && !(propertyDock->features() & QDockWidget::DockWidgetClosable)
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
    auto *collapseOutput = window.findChild<QPushButton *>("outputCollapseButton");
    if (!check(window.findChild<QWidget *>("outputBody")->isHidden()
               && collapseOutput->text() == text(u"\u5c55\u5f00"),
               "Mesh workspace must initially use compact global output")) return 64;
    collapseOutput->click();
    app.processEvents();
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
    // Ribbon: one unified entry per preprocess page, all view tools retained.
    auto *meshManagementPanel = preprocess->panelByObjectName("meshManagementGroup");
    auto *trajectoryPanel = preprocess->panelByObjectName("trajectoryGroup");
    auto *viewPanel = preprocess->panelByObjectName("viewGroup");
    auto *meshManagementAction = window.findChild<QAction *>("meshAction");
    auto *trajectoryAction = window.findChild<QAction *>("curveAction");
    if (!check(meshManagementPanel && meshManagementPanel->actions().size() == 1
               && trajectoryPanel && trajectoryPanel->actions().size() == 1
               && meshManagementAction
               && meshManagementAction->property("ribbonGroup").toString() == "meshManagementGroup"
               && meshManagementAction->text() == text(u"\u7f51\u683c\u7ba1\u7406")
               && trajectoryAction
               && trajectoryAction->property("ribbonGroup").toString() == "trajectoryGroup"
               && trajectoryAction->text() == text(u"\u5f39\u9053\u8bbe\u7f6e")
               && viewPanel && viewPanel->actions().size() == 8,
               "Preprocess Ribbon must expose one unified entry per page plus the view tools")) return 18;
    if (!check(window.findChild<QAction *>("showStartupAction")
               && window.findChild<QAction *>("showStartupAction")->isEnabled(),
               "Workbench must expose a return-to-startup entry")) return 56;
    auto *postprocessCategory = window.findChild<SARibbonCategory *>("postprocessAnalysisCategory");
    auto *helpRibbonCategory = window.findChild<SARibbonCategory *>("helpCategory");
    if (!check(postprocessCategory && !postprocessCategory->panelByObjectName("toolsGroup")
               && helpRibbonCategory && helpRibbonCategory->panelByObjectName("appearanceGroup")
               && helpRibbonCategory->panelByObjectName("layoutToolsGroup")
               && helpRibbonCategory->panelByObjectName("helpInfoGroup"),
               "Global appearance, layout and help tools must be centralized under Help")) return 19;
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
    bool batchPlanCommandsDisabled = true;
    for (auto *button : window.findChild<QWidget *>("batchPlanPage")->findChildren<QPushButton *>())
        batchPlanCommandsDisabled = batchPlanCommandsDisabled && !button->isEnabled();
    if (!check(batchPlanCommandsDisabled,
               "BatchPlan placeholder actions must remain disabled until its service exists")) return 40;
    if (!check(calculationNode->child(0)->child(0)->childCount() == 3
               && window.findChild<QTableWidget *>("stateTable")->rowCount() == 3
               && window.findChild<QPushButton *>("addStateButton")
               && window.findChild<QPushButton *>("removeStateButton"),
               "State management must expose the same editable state set in the tree and table")) return 76;
    auto *singleCalculationItem = calculationNode->child(0)->child(2);
    auto *batchCalculationItem = calculationNode->child(0)->child(3);
    projectTree->setCurrentItem(singleCalculationItem);
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "singleCalculationPage"
               && window.findChild<QPushButton *>("submitSingleCalculation")
               && window.findChild<QComboBox *>("singleCaseCombo"),
               "Single calculation page did not expose task generation controls")) return 74;
    auto *singleCase = window.findChild<QComboBox *>("singleCaseCombo");
    auto *singleMach = window.findChild<QDoubleSpinBox *>("singleMachSpin");
    singleCase->setCurrentIndex(1);
    app.processEvents();
    if (!check(singleMach->value() == 1.2,
               "Selecting a condition must load that condition's calculation parameters")) return 77;
    projectTree->setCurrentItem(batchCalculationItem);
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "batchCalculationPage"
               && window.findChild<QPushButton *>("addBatchCase")
               && window.findChild<QTableWidget *>("batchCalculationTable")->columnCount() == 6,
               "Batch calculation page did not expose plan editing controls")) return 75;
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
    auto *taskLogSplitter = window.findChild<QSplitter *>("taskMonitorContentSplitter");
    auto *taskLogView = window.findChild<QPlainTextEdit *>("taskRuntimeLogView");
    auto *taskLogFilter = window.findChild<QComboBox *>("taskLogStreamFilter");
    auto *appendTaskLog = window.findChild<QPushButton *>("appendTaskLogMockButton");
    auto *updateTaskProgress = window.findChild<QPushButton *>("updateTaskProgressMockButton");
    if (!check(taskLogSplitter && taskLogSplitter->count() == 2
               && taskLogSplitter->widget(0)->objectName() == "convergenceCurvePanel"
               && taskLogSplitter->widget(1)->objectName() == "taskRuntimeLogPanel"
               && taskLogView && taskLogView->isReadOnly()
               && taskLogView->lineWrapMode() == QPlainTextEdit::NoWrap
               && taskLogView->toPlainText().contains("Task_001")
               && taskLogFilter && taskLogFilter->count() == 3,
               "Task monitor must place a selectable fixed-font runtime log below its curve")) return 73;
    taskLogFilter->setCurrentIndex(taskLogFilter->findData("stderr"));
    app.processEvents();
    if (!check(taskLogView->toPlainText().contains("Warning")
               && !taskLogView->toPlainText().contains("Solver started"),
               "stderr filter changed or mixed the task log source data")) return 74;
    taskLogFilter->setCurrentIndex(taskLogFilter->findData("all"));
    app.processEvents();
    const int beforeAppendBlocks = taskLogView->document()->blockCount();
    appendTaskLog->click();
    app.processEvents();
    if (!check(taskLogView->document()->blockCount() == beforeAppendBlocks + 1
               && taskLogView->toPlainText().contains(text(u"Mock \u8ffd\u52a0")),
               "Line-oriented mock output was not appended incrementally")) return 75;
    const int beforeProgressBlocks = taskLogView->document()->blockCount();
    updateTaskProgress->click();
    app.processEvents();
    if (!check(taskLogView->document()->blockCount() == beforeProgressBlocks
               && taskLogView->toPlainText().contains("same-line update mock"),
               "Progress mock did not update the existing line in place")) return 76;
    projectTree->setCurrentItem(batchRunItem->child(1));
    app.processEvents();
    if (!check(window.findChild<QLabel *>("taskMonitorTitle")->text().contains("Task_002")
               && taskLogView->toPlainText().contains("Task_002")
               && !taskLogView->toPlainText().contains("Task_001"),
               "Switching batch tasks left the previous task log visible")) return 77;
    projectTree->setCurrentItem(batchRunItem->child(2));
    app.processEvents();
    if (!check(window.findChild<QLabel *>("taskMonitorTitle")->text().contains("Task_003")
               && taskLogView->toPlainText().isEmpty()
               && !taskLogView->placeholderText().isEmpty(),
               "Task without logs did not show the task-specific empty state")) return 78;
    projectTree->setCurrentItem(batchTaskItem);
    app.processEvents();
    if (!check(taskLogView->toPlainText().contains(text(u"Mock \u8ffd\u52a0")),
               "Returning to a task did not preserve its mock log state")) return 79;
    for (auto *action : window.findChildren<QAction *>()) {
        if (action->property("ribbonGroup").isValid()
            && !check(!action->icon().pixmap(32, 32).isNull(), "Missing embedded Ribbon icon")) return 20;
    }
    projectTree->setCurrentItem(preprocessNode->child(1));
    app.processEvents();
    if (!check(workspace->currentWidget()->objectName() == "trajectorySettingsPage",
               "Trajectory settings node did not open its unified page")) return 43;
    if (!check(!propertyDock->isHidden(),
               "Non-mesh workspace did not restore the shared property panel")) return 65;
    projectTree->setCurrentItem(preprocessNode->child(0));
    app.processEvents();
    if (!check(workspace->currentWidget() == meshPage && meshTable->rowCount() == 9
               && propertyDock->isHidden()
               && !window.findChild<QWidget *>("outputBody")->isHidden()
               && projectDock->width() >= 200
               && window.findChild<QWidget *>("meshViewerPanel")->width() > 0,
               "Mesh management group did not restore its persistent workspace")) return 63;
    app.processEvents();
    auto *saveUserLayout = window.findChild<QAction *>("saveUserLayoutAction");
    auto *restoreUserLayout = window.findChild<QAction *>("restoreUserLayoutAction");
    auto *restoreDefaultLayout = window.findChild<QAction *>("layoutAction");
    auto *meshMainSplitter = window.findChild<QSplitter *>("meshWorkspaceMainSplitter");
    auto *monitorLayoutSplitter = window.findChild<QSplitter *>("taskMonitorContentSplitter");
    if (!check(saveUserLayout && restoreUserLayout
               && restoreDefaultLayout && meshMainSplitter && monitorLayoutSplitter,
               "Help layout actions or adjustable page splitters are missing")) return 81;
    meshMainSplitter->setSizes({390, 610});
    monitorLayoutSplitter->setSizes({720, 280});
    saveUserLayout->trigger();
    app.processEvents();
    QSettings userLayout(QStringLiteral("AeroSimWorkbench"), QStringLiteral("UiLayout"));
    if (!check(restoreUserLayout->isEnabled()
               && userLayout.value("UserLayout/version").toInt() == 1,
               "Saving a user layout did not create the single persistent record")) return 82;
    const double savedMeshRatio = userLayout.value("UserLayout/meshLayout").toMap()
        .value("objectPanelRatio").toDouble();
    const double savedMonitorRatio = userLayout.value("UserLayout/monitorCurveRatio").toDouble();
    if (!check(savedMeshRatio >= 0.20 && savedMeshRatio <= 0.42
               && savedMonitorRatio >= 0.45 && savedMonitorRatio <= 0.82,
               "Saved layout ratios are outside the supported adaptive range")) return 85;
    meshMainSplitter->setSizes({245, 755});
    monitorLayoutSplitter->setSizes({470, 530});
    collapseOutput->click();
    window.ribbonBar()->setMinimumMode(true);
    restoreUserLayout->trigger();
    app.processEvents();
    const QList<int> restoredMeshSizes = meshMainSplitter->sizes();
    const QList<int> restoredMonitorSizes = monitorLayoutSplitter->sizes();
    if (!check(restoredMeshSizes.value(0) >= 245 && restoredMeshSizes.value(1) > 0
               && restoredMonitorSizes.value(0) > 0 && restoredMonitorSizes.value(1) >= 130
               && !window.ribbonBar()->isMinimumMode()
               && !window.findChild<QWidget *>("outputBody")->isHidden(),
               "Restoring the user layout did not recover relative split and collapse state")) return 83;
    restoreDefaultLayout->trigger();
    app.processEvents();
    const QList<int> defaultMeshSizes = meshMainSplitter->sizes();
    const double defaultMeshRatio = double(defaultMeshSizes.value(0))
        / qMax(1, defaultMeshSizes.value(0) + defaultMeshSizes.value(1));
    if (!check(defaultMeshRatio >= 0.54 && defaultMeshRatio <= 0.71
               && window.findChild<QWidget *>("outputBody")->isHidden()
               && userLayout.value("UserLayout/version").toInt() == 1,
               "Default layout must be safe without deleting the saved user layout")) return 84;
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
    const auto initialV2 = workflow.viewV2(aerosim::ScopeRef());
    if (!check(initialV2.ok(), "Workflow stub must expose a V2 view")) return 81;
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
    aerosim::WorkflowViewV2 view = workflow.viewV2(request.scope).value();
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
