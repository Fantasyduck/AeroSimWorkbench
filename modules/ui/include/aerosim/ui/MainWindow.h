#pragma once

#include "aerosim/ui/IntegrationPorts.h"
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

signals:
    void openPostProcessRequested();
    void showStartupRequested();
    void navigationRequested(const QString &pageId, const QString &objectId, const QString &fieldKey);
    void parameterDraftChanged(const QString &pageId, const QVariantMap &values);

private:
    struct StateDraft {
        double mach = 0.8;
        double angleOfAttack = 2.0;
        double temperature = 288.15;
        double altitude = 5000.0;
    };

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
    void selectNavigationNode(QTreeWidget *tree, const QString &objectId);
    void restoreLayout();
    void saveUserLayout();
    void restoreUserLayout();
    void constrainToAvailableScreen();
    void submitRun();
    void submitSingleCalculation();
    void addBatchCase();
    void removeBatchCase();
    void submitBatchCalculation();
    void applyStateToSingleCalculation(const QString &stateId);
    bool editCalculationParameters(QString &caseId, double &mach, double &aoa,
                                   double &temperature, int &mpi);
    void addState();
    void editState();
    void removeState();
    void refreshStateModel();
    void appendMessage(const QString &message);
    void appendLog(const QString &level, const QString &source, const QString &message,
                   const QString &scope = QString());
    void applyLogFilters();
    void setOutputScope(const QString &scope);
    void setOutputCollapsed(bool collapsed);
    void showTaskMonitor(const QString &taskId);
    void refreshTaskLog();
    void appendTaskLogMock(const QString &stream, const QString &message);
    void updateTaskProgressMock(const QString &message);
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
    QTreeWidget *taskTree_ = nullptr;
    QStackedWidget *workspaceStack_ = nullptr;
    QWidget *viewportPage_ = nullptr;
    QWidget *trajectorySettingsPage_ = nullptr;
    QWidget *parameterWorkspacePage_ = nullptr;
    QWidget *calculationOverviewPage_ = nullptr;
    QWidget *stateListPage_ = nullptr;
    QWidget *batchPlanPage_ = nullptr;
    QWidget *batchMonitorPage_ = nullptr;
    QWidget *taskMonitorPage_ = nullptr;
    QWidget *singleCalculationPage_ = nullptr;
    QWidget *batchCalculationPage_ = nullptr;
    QWidget *dataProcessPage_ = nullptr;
    QWidget *postprocessOverviewPage_ = nullptr;
    QHash<QString, QWidget *> workspacePages_;
    QHash<QString, QString> workspacePageModules_;
    QHash<QString, QString> moduleLastPages_;
    QTableWidget *stateTable_ = nullptr;
    QTableWidget *batchPlanTable_ = nullptr;
    QTableWidget *batchCalculationTable_ = nullptr;
    QTableWidget *outputLogTable_ = nullptr;
    QDockWidget *projectDock_ = nullptr;
    QDockWidget *propertyDock_ = nullptr;
    QDockWidget *outputDock_ = nullptr;
    QFormLayout *propertyForm_ = nullptr;
    QLabel *propertyTitle_ = nullptr;
    QLabel *parameterWorkspaceTitle_ = nullptr;
    QLabel *suggestion_ = nullptr;
    QLabel *monitorStatus_ = nullptr;
    QLabel *monitorDetails_ = nullptr;
    QLabel *taskMonitorTitle_ = nullptr;
    QLabel *taskCurveState_ = nullptr;
    QLabel *taskResourceSummary_ = nullptr;
    QLabel *outputTitle_ = nullptr;
    QLabel *status_ = nullptr;
    QProgressBar *progress_ = nullptr;
    QSplitter *taskMonitorContentSplitter_ = nullptr;
    QPushButton *draftReset_ = nullptr;
    QPushButton *outputScopeReset_ = nullptr;
    QPushButton *outputCollapse_ = nullptr;
    QComboBox *logLevelFilter_ = nullptr;
    QComboBox *logModuleFilter_ = nullptr;
    QComboBox *taskLogStreamFilter_ = nullptr;
    QLineEdit *logSearch_ = nullptr;
    QComboBox *singleCaseCombo_ = nullptr;
    QComboBox *singleOutputModeCombo_ = nullptr;
    QDoubleSpinBox *singleMachSpin_ = nullptr;
    QDoubleSpinBox *singleAoASpin_ = nullptr;
    QDoubleSpinBox *singleTemperatureSpin_ = nullptr;
    QSpinBox *singleMpiSpin_ = nullptr;
    QLabel *singleGenerationStatus_ = nullptr;
    QLabel *singleParametersSummary_ = nullptr;
    QLabel *batchGenerationStatus_ = nullptr;
    QLabel *batchRunSummary_ = nullptr;
    QProgressBar *batchRunProgress_ = nullptr;
    QPushButton *addStateButton_ = nullptr;
    QPushButton *editStateButton_ = nullptr;
    QPushButton *removeStateButton_ = nullptr;
    QTreeWidgetItem *statesNavigationItem_ = nullptr;
    QStringList stateIds_ = {QStringLiteral("State_001"), QStringLiteral("State_002"), QStringLiteral("State_003")};
    QHash<QString, StateDraft> stateDrafts_;
    quint64 batchPlanVersion_ = 0;
    QCheckBox *logAutoScroll_ = nullptr;
    QCheckBox *taskLogAutoScroll_ = nullptr;
    QPlainTextEdit *taskLogView_ = nullptr;
    QWidget *outputBody_ = nullptr;
    QString currentPage_;
    QString outputScope_ = QStringLiteral("current-project");
    QString currentTaskId_ = QStringLiteral("Task_001");
    int expandedOutputHeight_ = 140;
    bool outputCollapsed_ = false;
    bool projectEditingEnabled_ = false;
    bool syncingRibbonTree_ = false;
    bool meshWorkspaceActive_ = false;
    bool propertyDockVisibleBeforeMesh_ = true;
    int projectDockWidth_ = 260;
    // Embedded parameter editors hide the shared property dock; remember the
    // previous visibility so main's non-mesh pages restore it afterwards.
    bool parameterDockHidden_ = false;
    bool propertyDockVisibleBeforeParameter_ = true;
    bool meshOutputInitialCollapseApplied_ = false;
    QHash<QString, QVariantMap> drafts_;
    QHash<QString, QVector<QPair<QString, QString>>> taskLogEntries_;
    QHash<QString, int> taskLogScrollPositions_;
};

} // namespace aerosim
