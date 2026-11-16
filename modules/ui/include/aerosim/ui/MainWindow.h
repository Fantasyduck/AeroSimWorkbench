#pragma once

#include "aerosim/ui/IntegrationPorts.h"
#include "aerosim/ui/UiLayoutConfig.h"
#include <SARibbonMainWindow.h>
#include <QHash>
#include <QPointer>
#include <QPair>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QFormLayout;
class QDockWidget;
class QEvent;
class QObject;

namespace aerosim {

class CalculationTaskPage;
class ComputeMonitorPage;
class ResultDataTablePage;
class ResultSummaryTablePage;

// UI shell: business validation and persistence remain in injected services.
class MainWindow final : public SARibbonMainWindow, public IWidgetHost {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    QWidget *windowWidget() override { return this; }
    bool insertWidget(WidgetSlot slot, QWidget *widget) override;
    void bindServices(const ServiceBindings &services) override;
    void setRunContext(const RunRequest &request);
    void displayWorkflow(const WorkflowViewV2 &view);
    void displayTaskEvent(const TaskEvent &event);

    // Redirect where the layout/extras preferences are persisted. Defaults to
    // ("AeroSimWorkbench", "UiLayout"); the framework regression tests call
    // this with an isolated scope so they never read or overwrite the real user
    // layout. Must be set before triggering save/restore.
    void setLayoutSettingsScope(const QString &organization, const QString &application);

    // Redirect where the layout/extras preferences are persisted. Defaults to
    // ("AeroSimWorkbench", "UiLayout"); the framework regression tests call
    // this with an isolated scope so they never read or overwrite the real user
    // layout. Must be set before triggering save/restore.

    // The live monitoring page is retained as a reusable assembly slot for the
    // visualization module's curve host, but it is no longer a workspace-tree
    // navigation destination in the main window (that entry has been removed).
    ComputeMonitorPage *monitorPage() const { return monitorPage_; }

    // The two pages under 智能数据处理. They are created with the workspace but
    // stay behind abstract result ports, so the window never learns what a
    // monitor log or a chart is. The assembly layer injects the concrete reader
    // and curve view; a build without the visualization module leaves both
    // null and the pages simply show their empty state.
    ResultDataTablePage *resultDataTablePage() const { return resultDataTablePage_; }
    ResultSummaryTablePage *resultSummaryTablePage() const { return resultSummaryTablePage_; }

signals:
    void openPostProcessRequested();
    void showStartupRequested();
    void navigationRequested(const QString &pageId, const QString &objectId, const QString &fieldKey);
    void parameterDraftChanged(const QString &pageId, const QVariantMap &values);

private:
    void createRibbon();
    void applyWindowTheme(const QString &id);
    void createWorkspace();
    void selectPage(const QString &pageId);
    void switchNavigationForCategory(int index);
    void handleNavigationSelection(QTreeWidgetItem *item);
    void showParameterDialog(const QString &pageId, const QString &title);
    void showWorkspacePage(const QString &pageId);
    void showModuleWorkspace(const QString &moduleId);
    void setMeshWorkspaceActive(bool active);
    void setCalculationWorkspaceActive(bool active);
    void selectNavigationNode(QTreeWidget *tree, const QString &objectId);
    void restoreLayout();
    void saveUserLayout();
    void restoreUserLayout();
    void constrainToAvailableScreen();
    void submitRun();
    void appendMessage(const QString &message);
    void appendLog(const QString &level, const QString &source, const QString &message,
                   const QString &scope = QString());
    void applyLogFilters();
    void setOutputScope(const QString &scope);
    void setOutputCollapsed(bool collapsed);
    QWidget *createSlot(WidgetSlot slot, const QString &text);
    void showOutput(int index);
    bool eventFilter(QObject *watched, QEvent *event) override;

    QHash<int, QVBoxLayout *> slotLayouts_;
    QHash<int, QPointer<QWidget>> insertedWidgets_;
    QVector<QMetaObject::Connection> serviceConnections_;
    QPointer<IWorkflowService> workflow_;
    QPointer<ITaskService> tasks_;
    RunRequest runRequest_;
    bool hasRunContext_ = false;
    WorkflowViewV2 workflowView_;
    QAction *runAction_ = nullptr;
    QAction *restoreUserLayoutAction_ = nullptr;
    QTreeWidget *projectTree_ = nullptr;
    QTreeWidget *workflowTree_ = nullptr;
    QStackedWidget *workspaceStack_ = nullptr;
    QWidget *viewportPage_ = nullptr;
    QWidget *trajectorySettingsPage_ = nullptr;
    QWidget *parameterWorkspacePage_ = nullptr;
    CalculationTaskPage *singleCalculationPage_ = nullptr;
    CalculationTaskPage *batchCalculationPage_ = nullptr;
    QWidget *dataProcessPage_ = nullptr;
    QWidget *postprocessOverviewPage_ = nullptr;
    ComputeMonitorPage *monitorPage_ = nullptr;
    ResultDataTablePage *resultDataTablePage_ = nullptr;
    ResultSummaryTablePage *resultSummaryTablePage_ = nullptr;
    QStackedWidget *intelligentStack_ = nullptr;
    // page id -> index inside intelligentStack_. Several ids deliberately map to
    // the same widget: the module entry point and the first result page are the
    // same screen, and keeping them distinct without a second widget avoids a
    // duplicate page the user would have to wonder about.
    QHash<QString, int> intelligentPageIndex_;
    QHash<QString, QWidget *> workspacePages_;
    QHash<QString, QString> workspacePageModules_;
    QHash<QString, QString> moduleLastPages_;
    QTableWidget *outputLogTable_ = nullptr;
    QDockWidget *projectDock_ = nullptr;
    QDockWidget *propertyDock_ = nullptr;
    QDockWidget *outputDock_ = nullptr;
    QFormLayout *propertyForm_ = nullptr;
    QLabel *propertyTitle_ = nullptr;
    QLabel *parameterWorkspaceTitle_ = nullptr;
    QLabel *suggestion_ = nullptr;
    QLabel *outputTitle_ = nullptr;
    QLabel *status_ = nullptr;
    QPushButton *draftReset_ = nullptr;
    QPushButton *outputScopeReset_ = nullptr;
    QPushButton *outputCollapse_ = nullptr;
    QComboBox *logLevelFilter_ = nullptr;
    QComboBox *logModuleFilter_ = nullptr;
    QLineEdit *logSearch_ = nullptr;
    QCheckBox *logAutoScroll_ = nullptr;
    QWidget *outputBody_ = nullptr;
    QString currentPage_;
    QString outputScope_ = QStringLiteral("current-project");
    int expandedOutputHeight_ = uimetrics::outputExpandedHeight();
    bool outputCollapsed_ = false;
    bool projectEditingEnabled_ = false;
    bool syncingRibbonTree_ = false;
    bool meshWorkspaceActive_ = false;
    bool calculationWorkspaceActive_ = false;
    bool propertyDockVisibleBeforeMesh_ = true;
    bool propertyDockVisibleBeforeCalculation_ = true;
    bool outputDockVisibleBeforeCalculation_ = true;
    int projectDockWidth_ = uimetrics::projectDockWidth();
    // Persistence scope for layout/extras (see setLayoutSettingsScope).
    QString layoutSettingsOrg_ = QStringLiteral("AeroSimWorkbench");
    QString layoutSettingsApp_ = QStringLiteral("UiLayout");
    // Embedded parameter editors hide the shared property dock; remember the
    // previous visibility so main's non-mesh pages restore it afterwards.
    bool parameterDockHidden_ = false;
    bool propertyDockVisibleBeforeParameter_ = true;
    bool meshOutputInitialCollapseApplied_ = false;
    QHash<QString, QVariantMap> drafts_;
};

} // namespace aerosim
