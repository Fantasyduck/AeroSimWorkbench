#pragma once

#include "aerosim/contracts/TaskModels.h"
#include "aerosim/ui/monitor/MonitorCurveWidget.h"
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QVariantMap>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

namespace aerosim {

// One page for standalone runs and batch children. A presentation cache only;
// scheduling, persistent records and curve rendering belong to their owners.
class ComputeMonitorPage final : public QWidget {
    Q_OBJECT
public:
    explicit ComputeMonitorPage(QWidget *parent = nullptr);
    ~ComputeMonitorPage() override;
    void upsertTask(const TaskRecord &record, const QVariantMap &parameters = {});
    bool displayTaskEvent(const TaskEvent &event);
    void appendLog(const LogEntryV2 &entry, const QString &attemptId);
    void appendMonitorSample(const MonitorSampleV2 &sample, const QString &attemptId);
    bool insertCurveWidget(MonitorCurveWidget *widget);
    bool selectTask(const QString &taskId);
    bool selectBatch(const QString &batchId);
    void clearSelection();
    void setTaskNavigationVisible(bool visible);
    TaskRecord taskRecord(const QString &id) const;
    QVariantMap taskParameters(const QString &id) const;
    QString selectedBatchId() const { return selectedBatch_; }
    void clearTasks();
    QString selectedTaskId() const { return selectedTask_; }

signals:
    void taskSelected(const QString &taskId);
    void selectionContextChanged(const QString &taskId, const QString &batchId);
    void taskChanged(const aerosim::TaskRecord &record);
    void tasksCleared();

private:
    struct TaskView {
        TaskRecord record;
        QVariantMap parameters;
        QStringList logs;
        QVector<MonitorSampleV2> samples;
        QHash<QString, quint64> sampleSequences;
        QSet<QString> retiredAttempts;
        quint64 eventSequence = 0;
        TaskProgress progress;
        bool hasProgress = false;
    };
    void updateTree(const QString &id);
    void selectionChanged();
    void refreshDetails(bool refreshParameters = true);
    void refreshBatch();
    void addLog(TaskView &task, const QString &text);
    void resetCurve();
    bool matchesAttempt(const TaskView &task, const QString &attemptId) const;

    QHash<QString, TaskView> tasks_;
    QHash<QString, QTreeWidgetItem *> taskItems_;
    QHash<QString, QTreeWidgetItem *> batchItems_;
    QTreeWidget *tree_;
    QTreeWidgetItem *singleRoot_;
    QTreeWidgetItem *batchRoot_;
    QLabel *title_;
    QLabel *status_;
    QLabel *metrics_;
    QLabel *parametersTitle_;
    QLabel *curveHint_;
    QLabel *logHint_;
    QProgressBar *progress_;
    QTableWidget *parameters_;
    QTableWidget *children_;
    QPlainTextEdit *log_;
    QVBoxLayout *curveLayout_;
    QPointer<MonitorCurveWidget> curve_;
    QString selectedTask_;
    QString selectedBatch_;
};

} // namespace aerosim
