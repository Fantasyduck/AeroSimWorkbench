#pragma once

#include "aerosim/ui/IntegrationPorts.h"
#include <SARibbonMainWindow.h>
#include <QHash>
#include <QPointer>
#include <QVariantMap>
#include <QVector>

class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QFormLayout;
class QDockWidget;

namespace aerosim {
class ComputeMonitorPage;

// UI shell: business validation and persistence remain in injected services.
class MainWindow final : public SARibbonMainWindow, public IWidgetHost {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    QWidget *windowWidget() override { return this; }
    bool insertWidget(WidgetSlot slot, QWidget *widget) override;
    void bindServices(const ServiceBindings &services) override;
    void setRunContext(const RunRequest &request);
    void displayWorkflow(const WorkflowView &view);
    void displayTaskEvent(const TaskEvent &event);
    ComputeMonitorPage *monitorPage() const { return monitorPage_; }
    void showComputeMonitor();

signals:
    void openPostProcessRequested();
    void showStartupRequested();
    void navigationRequested(const QString &pageId, const QString &objectId, const QString &fieldKey);
    void parameterDraftChanged(const QString &pageId, const QVariantMap &values);

private:
    void createRibbon();
    void applyWindowTheme(const QString &id);
    void createWorkspace();
    void updateMonitorNavigation(const TaskRecord &record);
    void clearMonitorNavigation();
    void syncMonitorSelection(const QString &taskId, const QString &batchId);
    void selectPage(const QString &pageId);
    void switchNavigationForCategory(int index);
    void handleNavigationSelection(QTreeWidgetItem *item);
    void showParameterDialog(const QString &pageId, const QString &title);
    void showWorkspacePage(const QString &pageId);
    void selectNavigationNode(QTreeWidget *tree, const QString &objectId);
    void restoreLayout();
    void submitRun();
    void appendMessage(const QString &message);
    void appendLog(const QString &level, const QString &source, const QString &message,
                   const QString &scope = QString());
    void applyLogFilters();
    void setOutputScope(const QString &scope);
    void setOutputCollapsed(bool collapsed);
    QWidget *createSlot(WidgetSlot slot, const QString &text);
    void showOutput(int index);

    QHash<int, QVBoxLayout *> slotLayouts_;
    QHash<int, QPointer<QWidget>> insertedWidgets_;
    QVector<QMetaObject::Connection> serviceConnections_;
    QPointer<IWorkflowService> workflow_;
    QPointer<ITaskService> tasks_;
    RunRequest runRequest_;
    bool hasRunContext_ = false;
    WorkflowView workflowView_;
    QAction *runAction_ = nullptr;
    QTreeWidget *projectTree_ = nullptr;
    QTreeWidget *workflowTree_ = nullptr;
    ComputeMonitorPage *monitorPage_ = nullptr;
    QWidget *monitorWorkspace_ = nullptr;
    QTreeWidgetItem *singleRuns_ = nullptr;
    QTreeWidgetItem *batchRuns_ = nullptr;
    QHash<QString, QTreeWidgetItem *> monitorTaskItems_;
    QHash<QString, QTreeWidgetItem *> monitorBatchItems_;
    QHash<QString, QString> monitorAttempts_;
    quint64 serviceGeneration_ = 0;
    bool monitorActive_ = false;
    bool propertiesBeforeMonitor_ = true;
    bool outputCollapsedBeforeMonitor_ = false;
    QStackedWidget *workspaceStack_ = nullptr;
    QWidget *viewportPage_ = nullptr;
    QWidget *stateListPage_ = nullptr;
    QWidget *batchPlanPage_ = nullptr;
    QWidget *dataProcessPage_ = nullptr;
    QHash<QString, QWidget *> workspacePages_;
    QTableWidget *stateTable_ = nullptr;
    QTableWidget *batchPlanTable_ = nullptr;
    QTableWidget *outputLogTable_ = nullptr;
    QDockWidget *projectDock_ = nullptr;
    QDockWidget *propertyDock_ = nullptr;
    QDockWidget *outputDock_ = nullptr;
    QFormLayout *propertyForm_ = nullptr;
    QLabel *propertyTitle_ = nullptr;
    QLabel *propertyNotice_ = nullptr;
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
    int expandedOutputHeight_ = 140;
    bool outputCollapsed_ = false;
    bool projectEditingEnabled_ = false;
    bool syncingRibbonTree_ = false;
    QHash<QString, QVariantMap> drafts_;
};

} // namespace aerosim
