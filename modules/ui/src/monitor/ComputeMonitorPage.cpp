#include "aerosim/ui/monitor/ComputeMonitorPage.h"

#include <QHeaderView>
#include <QFont>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTextCursor>
#include <QThread>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <cmath>

namespace aerosim {
namespace {
constexpr int kLogLimit = 2000;
constexpr int kSampleLimit = 5000;
QString statusText(TaskStatus status)
{
    switch (status) {
    case TaskStatus::Preparing: return QStringLiteral(u"等待 / 准备中");
    case TaskStatus::Running: return QStringLiteral(u"运行中");
    case TaskStatus::Paused: return QStringLiteral(u"已暂停");
    case TaskStatus::Finished: return QStringLiteral(u"已完成");
    case TaskStatus::Failed: return QStringLiteral(u"失败");
    case TaskStatus::Cancelled: return QStringLiteral(u"已取消");
    default: return QStringLiteral(u"未知");
    }
}
QColor statusColor(TaskStatus status)
{
    if (status == TaskStatus::Failed) return QColor("#b3261e");
    if (status == TaskStatus::Finished) return QColor("#217346");
    if (status == TaskStatus::Running) return QColor("#165dba");
    return QColor("#526174");
}
QString valueOrDash(const QString &value) { return value.isEmpty() ? QStringLiteral(u"—") : value; }
QLabel *label(const QString &text, QWidget *parent, const char *name)
{
    auto *result = new QLabel(text, parent);
    result->setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    result->setObjectName(name);
    result->setTextFormat(Qt::PlainText);
    result->setWordWrap(true);
    return result;
}
void configureTable(QTableWidget *table, const QStringList &headers)
{
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
}
}

ComputeMonitorPage::ComputeMonitorPage(QWidget *parent) : QWidget(parent)
{
    setObjectName("computeMonitorPage");
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    auto *split = new QSplitter(Qt::Horizontal, this);
    split->setObjectName("monitorHorizontalSplit");
    split->setChildrenCollapsible(false);
    layout->addWidget(split);
    tree_ = new QTreeWidget(split);
    tree_->setObjectName("monitorTaskTree");
    tree_->setHeaderLabels({QStringLiteral(u"计算任务"), QStringLiteral(u"状态")});
    tree_->setMinimumWidth(210);
    tree_->setColumnWidth(0, 180);
    singleRoot_ = new QTreeWidgetItem(tree_, {QStringLiteral(u"单次计算任务")});
    batchRoot_ = new QTreeWidgetItem(tree_, {QStringLiteral(u"批处理任务")});
    singleRoot_->setExpanded(true);
    batchRoot_->setExpanded(true);
    auto *detail = new QWidget(split);
    detail->setMinimumWidth(380);
    auto *detailLayout = new QVBoxLayout(detail);
    title_ = label(QStringLiteral(u"请选择计算任务"), detail, "monitorTaskTitle");
    title_->setStyleSheet("font-size:16px; font-weight:600;");
    status_ = label({}, detail, "monitorTaskStatus");
    metrics_ = label({}, detail, "monitorTaskMetrics");
    progress_ = new QProgressBar(detail);
    progress_->setObjectName("monitorTaskProgress");
    detailLayout->addWidget(title_);
    detailLayout->addWidget(status_);
    detailLayout->addWidget(metrics_);
    detailLayout->addWidget(progress_);
    auto *body = new QSplitter(Qt::Horizontal, detail);
    body->setChildrenCollapsible(false);
    detailLayout->addWidget(body, 1);
    auto *config = new QWidget(body);
    config->setMinimumWidth(210);
    auto *configLayout = new QVBoxLayout(config);
    configLayout->setContentsMargins(0, 0, 4, 0);
    parametersTitle_ = label(QStringLiteral(u"运行参数快照（只读）"), config, "monitorParametersTitle");
    configLayout->addWidget(parametersTitle_);
    parameters_ = new QTableWidget(config);
    parameters_->setObjectName("monitorParameters");
    configureTable(parameters_, {QStringLiteral(u"参数"), QStringLiteral(u"值")});
    configLayout->addWidget(parameters_, 1);
    children_ = new QTableWidget(config);
    children_->setObjectName("monitorBatchChildren");
    configureTable(children_, {QStringLiteral(u"子任务（点击查看）"), QStringLiteral(u"状态")});
    configLayout->addWidget(children_, 1);
    auto *vertical = new QSplitter(Qt::Vertical, body);
    vertical->setObjectName("monitorCurveLogSplit");
    vertical->setChildrenCollapsible(false);
    auto *curveHost = new QWidget(vertical);
    curveLayout_ = new QVBoxLayout(curveHost);
    curveLayout_->setContentsMargins(0, 0, 0, 0);
    curveLayout_->addWidget(label(QStringLiteral(u"监控曲线"), curveHost, "monitorCurveTitle"));
    curveHint_ = label(QStringLiteral(u"等待邱的曲线组件接入\n选择任务后将显示该任务的数据"), curveHost, "monitorCurveHint");
    curveHint_->setAlignment(Qt::AlignCenter);
    curveHint_->setMinimumHeight(120);
    curveHint_->setStyleSheet("background:#f0f5fa; color:#536a82; border:1px solid #dce2eb;");
    curveLayout_->addWidget(curveHint_, 1);
    auto *logHost = new QWidget(vertical);
    auto *logLayout = new QVBoxLayout(logHost);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logHint_ = label(QStringLiteral(u"运行日志 · 最近 2000 行"), logHost, "monitorLogHint");
    logLayout->addWidget(logHint_);
    log_ = new QPlainTextEdit(logHost);
    log_->setObjectName("monitorTaskLog");
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(kLogLimit);
    log_->setMinimumHeight(100);
    logLayout->addWidget(log_);
    vertical->setSizes({350, 180});
    body->setSizes({260, 600});
    split->setSizes({280, 900});
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this] { selectionChanged(); });
    connect(children_, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (auto *item = children_->item(row, 0)) selectTask(item->data(Qt::UserRole).toString());
    });
    refreshDetails();
}

bool ComputeMonitorPage::matchesAttempt(const TaskView &task, const QString &attemptId) const
{
    return task.record.attemptId == attemptId;
}

ComputeMonitorPage::~ComputeMonitorPage()
{
    // Child teardown must not call selectionChanged after the cache is gone.
    disconnect(tree_, nullptr, this, nullptr);
    disconnect(children_, nullptr, this, nullptr);
}

void ComputeMonitorPage::upsertTask(const TaskRecord &record, const QVariantMap &parameters)
{
    if (record.taskId.isEmpty()) return;
    auto &task = tasks_[record.taskId];
    if (task.retiredAttempts.contains(record.attemptId)) return;
    if (task.record.attemptId != record.attemptId) {
        auto retired = task.retiredAttempts;
        retired.insert(task.record.attemptId);
        task = TaskView{};
        task.retiredAttempts = retired;
    }
    const auto liveStatus = task.record.status;
    task.record = record;
    if (task.eventSequence) task.record.status = liveStatus;
    task.parameters = parameters;
    updateTree(record.taskId);
    emit taskChanged(task.record);
    if (selectedTask_ == record.taskId) {
        refreshDetails();
        log_->setPlainText(task.logs.join('\n'));
        log_->moveCursor(QTextCursor::End);
        resetCurve();
    } else if (!selectedBatch_.isEmpty()) refreshBatch();
}

void ComputeMonitorPage::updateTree(const QString &id)
{
    const auto &record = tasks_[id].record;
    auto *parent = singleRoot_;
    if (!record.batchId.isEmpty()) {
        if (!batchItems_.contains(record.batchId)) {
            auto *batch = new QTreeWidgetItem(batchRoot_, {record.batchId});
            batch->setData(0, Qt::UserRole + 1, record.batchId);
            batch->setExpanded(true);
            batchItems_.insert(record.batchId, batch);
        }
        parent = batchItems_[record.batchId];
    }
    auto *item = taskItems_.value(id);
    if (!item) {
        item = new QTreeWidgetItem(parent, {id});
        item->setData(0, Qt::UserRole, id);
        taskItems_.insert(id, item);
    } else if (item->parent() != parent) {
        QSignalBlocker block(tree_);
        item->parent()->takeChild(item->parent()->indexOfChild(item));
        parent->addChild(item);
        if (selectedTask_ == id) tree_->setCurrentItem(item);
    }
    item->setText(1, statusText(record.status));
    item->setForeground(1, statusColor(record.status));
    item->setToolTip(0, QStringLiteral(u"任务：%1\n工况：%2\n运行次数标识：%3")
                     .arg(id, valueOrDash(record.caseId), valueOrDash(record.attemptId)));
}

bool ComputeMonitorPage::selectTask(const QString &id)
{
    if (!taskItems_.contains(id)) return false;
    auto *item = taskItems_.value(id);
    item->parent()->setExpanded(true);
    tree_->setCurrentItem(item);
    tree_->scrollToItem(item);
    return true;
}

void ComputeMonitorPage::setTaskNavigationVisible(bool visible)
{
    tree_->setVisible(visible);
}

TaskRecord ComputeMonitorPage::taskRecord(const QString &id) const
{
    return tasks_.value(id).record;
}

QVariantMap ComputeMonitorPage::taskParameters(const QString &id) const
{
    return tasks_.value(id).parameters;
}

bool ComputeMonitorPage::selectBatch(const QString &id)
{
    auto *item = batchItems_.value(id);
    if (!item) return false;
    tree_->setCurrentItem(item);
    return true;
}

void ComputeMonitorPage::clearSelection()
{
    tree_->setCurrentItem(nullptr);
}

void ComputeMonitorPage::selectionChanged()
{
    auto *item = tree_->currentItem();
    selectedTask_ = item ? item->data(0, Qt::UserRole).toString() : QString();
    selectedBatch_ = item ? item->data(0, Qt::UserRole + 1).toString() : QString();
    refreshDetails();
    log_->setPlainText(selectedTask_.isEmpty() ? QString() : tasks_[selectedTask_].logs.join('\n'));
    log_->moveCursor(QTextCursor::End);
    resetCurve();
    emit taskSelected(selectedTask_);
    emit selectionContextChanged(selectedTask_, selectedBatch_);
}

void ComputeMonitorPage::refreshDetails(bool refreshParameters)
{
    if (refreshParameters) parameters_->setRowCount(0);
    parameters_->setVisible(selectedBatch_.isEmpty());
    parametersTitle_->setText(selectedBatch_.isEmpty()
        ? QStringLiteral(u"运行参数快照（只读）") : QStringLiteral(u"批次子任务"));
    children_->setVisible(!selectedBatch_.isEmpty());
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setFormat(QStringLiteral(u"暂无进度"));
    metrics_->clear();
    logHint_->setText(QStringLiteral(u"运行日志 · 最近 2000 行"));
    if (!selectedBatch_.isEmpty()) { refreshBatch(); return; }
    if (selectedTask_.isEmpty()) {
        title_->setText(QStringLiteral(u"请选择计算任务"));
        status_->setText(tasks_.isEmpty() ? QStringLiteral(u"尚无任务；等待运行服务提供任务记录") : QStringLiteral(u"在左侧选择单次任务或批处理子任务"));
        return;
    }
    const auto &task = tasks_[selectedTask_];
    title_->setText(task.record.taskId);
    status_->setText(QStringLiteral(u"状态：%1    工况：%2    批次：%3")
                    .arg(statusText(task.record.status), valueOrDash(task.record.caseId), valueOrDash(task.record.batchId)));
    if (refreshParameters) {
        QVariantMap rows = task.parameters;
        rows.insert(QStringLiteral(u"输入快照"), valueOrDash(task.record.inputSnapshot.snapshotId));
        rows.insert(QStringLiteral(u"求解器"), valueOrDash(task.record.solverId));
        rows.insert(QStringLiteral(u"运行后端"), valueOrDash(task.record.backendId));
        rows.insert(QStringLiteral(u"运行次数标识"), valueOrDash(task.record.attemptId));
        parameters_->setRowCount(rows.size());
        int row = 0;
        for (auto it = rows.cbegin(); it != rows.cend(); ++it, ++row) {
            parameters_->setItem(row, 0, new QTableWidgetItem(it.key()));
            auto *value = new QTableWidgetItem(it.value().toString());
            value->setToolTip(value->text());
            parameters_->setItem(row, 1, value);
        }
    }
    if (task.hasProgress) {
        metrics_->setText(QStringLiteral(u"迭代：%1 / %2    残差：%3")
                          .arg(task.progress.currentStep).arg(task.progress.totalSteps).arg(task.progress.residual, 0, 'g', 6));
        if (task.progress.totalSteps > 0) {
            progress_->setValue(int(qBound(0.0, 100.0 * task.progress.currentStep / task.progress.totalSteps, 100.0)));
            progress_->setFormat("%p%");
        }
    } else metrics_->setText(QStringLiteral(u"尚无迭代 / 残差数据"));
}

void ComputeMonitorPage::refreshBatch()
{
    if (selectedBatch_.isEmpty()) return;
    int total = 0, finished = 0, failed = 0, running = 0;
    QStringList ids;
    for (auto it = tasks_.cbegin(); it != tasks_.cend(); ++it) {
        if (it->record.batchId != selectedBatch_) continue;
        ids.append(it.key());
        ++total;
        finished += it->record.status == TaskStatus::Finished;
        failed += it->record.status == TaskStatus::Failed;
        running += it->record.status == TaskStatus::Running;
    }
    ids.sort();
    title_->setText(QStringLiteral(u"批次：%1").arg(selectedBatch_));
    status_->setText(QStringLiteral(u"共 %1 项 · 运行 %2 · 完成 %3 · 失败 %4").arg(total).arg(running).arg(finished).arg(failed));
    metrics_->setText(QStringLiteral(u"完成比例按成功完成的子任务数统计；选择子任务查看配置、曲线和日志"));
    progress_->setValue(total ? 100 * finished / total : 0);
    progress_->setFormat(QStringLiteral(u"成功完成 %p%"));
    logHint_->setText(QStringLiteral(u"批次概览不混合子任务日志；请选择一个子任务"));
    children_->setRowCount(ids.size());
    for (int row = 0; row < ids.size(); ++row) {
        auto *item = new QTableWidgetItem(ids[row]);
        item->setData(Qt::UserRole, ids[row]);
        children_->setItem(row, 0, item);
        children_->setItem(row, 1, new QTableWidgetItem(statusText(tasks_[ids[row]].record.status)));
    }
}

void ComputeMonitorPage::resetCurve()
{
    if (curve_) {
        const auto task = tasks_.value(selectedTask_);
        curve_->replaceTask(selectedTask_, task.record.attemptId, task.samples);
    }
    curveHint_->setText(selectedBatch_.isEmpty()
        ? QStringLiteral(u"等待邱的曲线组件接入\n当前任务：%1 · 最近保留 %2 个采样")
          .arg(valueOrDash(selectedTask_)).arg(tasks_.value(selectedTask_).samples.size())
        : QStringLiteral(u"选择一个子任务查看曲线；此处不合并不同任务的曲线"));
}

bool ComputeMonitorPage::insertCurveWidget(MonitorCurveWidget *widget)
{
    if (!widget || curve_ || QThread::currentThread() != thread() || widget->thread() != thread()
        || widget->isAncestorOf(this) || isAncestorOf(widget)) return false;
    curveLayout_->addWidget(widget, 1);
    curve_ = widget;
    curveHint_->hide();
    connect(widget, &QObject::destroyed, curveHint_, [hint = curveHint_] { hint->show(); });
    resetCurve();
    return true;
}

void ComputeMonitorPage::addLog(TaskView &task, const QString &text)
{
    // Bound physical lines and per-message size as well as message count.
    const auto lines = text.left(65536).split('\n');
    for (const auto &line : lines) task.logs.append(line);
    while (task.logs.size() > kLogLimit) task.logs.removeFirst();
    if (selectedTask_ == task.record.taskId) log_->appendPlainText(text.left(65536));
}

bool ComputeMonitorPage::displayTaskEvent(const TaskEvent &event)
{
    if (event.taskId.isEmpty()) return false;
    if (!tasks_.contains(event.taskId)) {
        TaskRecord record;
        record.taskId = event.taskId;
        record.attemptId = event.attemptId;
        upsertTask(record);
    }
    auto &task = tasks_[event.taskId];
    if (!matchesAttempt(task, event.attemptId)) return false;
    if (event.sequence && event.sequence <= task.eventSequence) return false;
    if (!event.sequence && task.eventSequence) return false;
    task.eventSequence = event.sequence;
    if (event.status != TaskStatus::Unknown) task.record.status = event.status;
    if (event.type == TaskEventType::ProgressUpdated || event.type == TaskEventType::ResidualUpdated
        || event.progress.totalSteps > 0) {
        task.progress = event.progress;
        task.hasProgress = true;
    }
    if (!event.message.isEmpty()) addLog(task, event.timestamp.toLocalTime().toString("[HH:mm:ss] ") + event.message);
    updateTree(event.taskId);
    emit taskChanged(task.record);
    if (selectedTask_ == event.taskId) refreshDetails(false);
    else if (!selectedBatch_.isEmpty()) refreshBatch();
    return true;
}

void ComputeMonitorPage::appendLog(const LogEntryV2 &entry, const QString &attemptId)
{
    // The contract carries the attempt on the sample and the task in a TaskRef;
    // the explicit attemptId argument stays the authority so a stale delivery
    // can never attach itself to the current attempt by claiming a new one.
    auto it = tasks_.find(entry.task.taskId);
    if (it == tasks_.end() || !matchesAttempt(*it, attemptId)) return;
    addLog(*it, entry.timestamp.toLocalTime().toString("[HH:mm:ss] ") + entry.text);
}

void ComputeMonitorPage::appendMonitorSample(const MonitorSampleV2 &sample, const QString &attemptId)
{
    auto it = tasks_.find(sample.task.taskId);
    if (it == tasks_.end() || !matchesAttempt(*it, attemptId)) return;
    if (!std::isfinite(sample.value) || sample.metricKey.isEmpty()) return;
    // Each metric may have its own sequence stream. The series is identified by
    // metric key plus component: a solver that reports CD and, separately, the
    // upper and lower surface components of it produces three distinct curves,
    // and collapsing them onto the metric key alone would splice them together.
    const QString series = sample.component.isEmpty()
        ? sample.metricKey
        : sample.metricKey + QLatin1Char('/') + sample.component;
    auto &last = it->sampleSequences[series];
    if (sample.sequence && sample.sequence <= last) return;
    if (!sample.sequence && last) return;
    last = sample.sequence;
    it->samples.append(sample);
    const bool trimmed = it->samples.size() > kSampleLimit;
    if (trimmed) it->samples.remove(0, it->samples.size() - kSampleLimit);
    if (selectedTask_ != sample.task.taskId) return;
    if (curve_ && !trimmed) curve_->appendSample(sample);
    else resetCurve();
}

void ComputeMonitorPage::clearTasks()
{
    QSignalBlocker block(tree_);
    tree_->setCurrentItem(nullptr);
    qDeleteAll(singleRoot_->takeChildren());
    qDeleteAll(batchRoot_->takeChildren());
    taskItems_.clear();
    batchItems_.clear();
    tasks_.clear();
    selectionChanged();
    emit tasksCleared();
}

} // namespace aerosim
