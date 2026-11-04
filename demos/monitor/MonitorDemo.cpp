#include "MonitorDemo.h"
#include "aerosim/ui/monitor/ComputeMonitorPage.h"
#include <QTimer>

namespace aerosim {
void populateMonitorDemo(ComputeMonitorPage *page)
{
    const QStringList names = {"Run_001", "Run_002", "Task_001", "Task_002", "Task_003"};
    const TaskStatus states[] = {TaskStatus::Finished, TaskStatus::Running,
        TaskStatus::Finished, TaskStatus::Running, TaskStatus::Preparing};
    for (int i = 0; i < names.size(); ++i) {
        TaskRecord record;
        record.taskId = names[i];
        record.batchId = i >= 2 ? "BatchRun_001" : QString();
        record.attemptId = "ui-demo-1";
        record.caseId = QString("Case_%1").arg(i + 1);
        record.solverId = QStringLiteral(u"界面联动演示 · 未运行求解器");
        record.backendId = "UI demo";
        record.inputSnapshot.snapshotId = QString("demo-input-%1").arg(i + 1);
        record.status = states[i];
        page->upsertTask(record, {{QStringLiteral(u"马赫数"), 0.6 + 0.1 * i},
                                 {QStringLiteral(u"攻角（°）"), i * 2.0},
                                 {QStringLiteral(u"MPI 进程数"), 4 * (i + 1)}});
        LogEntry log;
        log.taskId = record.taskId;
        log.timestamp = QDateTime::currentDateTime();
        log.message = QStringLiteral(u"%1：界面演示数据，用于检查任务选择联动；没有启动计算。") .arg(record.taskId);
        page->appendLog(log, record.attemptId);
    }
    page->selectTask("Task_002");
    auto *timer = new QTimer(page);
    int step = 0;
    QObject::connect(timer, &QTimer::timeout, page, [page, timer, step]() mutable {
        ++step;
        for (const QString id : {"Task_002", "Run_002"}) {
            TaskEvent event;
            event.taskId = id;
            event.attemptId = "ui-demo-1";
            event.sequence = step;
            event.timestamp = QDateTime::currentDateTime();
            event.type = step == 60 ? TaskEventType::Finished : TaskEventType::ProgressUpdated;
            event.status = step == 60 ? TaskStatus::Finished : TaskStatus::Running;
            event.progress.currentStep = step;
            event.progress.totalSteps = 60;
            event.progress.residual = (id == "Task_002" ? 1.0 : 2.0) / (step + 1);
            event.message = QStringLiteral(u"[界面演示] %1 · 更新 %2/60").arg(id).arg(step);
            page->displayTaskEvent(event);
            MonitorSample sample;
            sample.taskId = id;
            sample.sequence = step;
            sample.timestamp = event.timestamp;
            sample.metric = "demo-residual";
            sample.value = event.progress.residual;
            page->appendMonitorSample(sample, event.attemptId);
        }
        if (step == 60) timer->stop();
    });
    timer->start(1000);
}
}
