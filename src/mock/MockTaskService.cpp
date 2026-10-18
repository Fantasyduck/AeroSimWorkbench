#include "MockTaskService.h"

#include <QThread>
#include <QTimer>
#include <QUuid>

namespace aerosim {

TaskRecord MockTaskService::record(const QString &taskId) const
{
    return records_.value(taskId);
}

void MockTaskService::handleTaskCommand(const TaskCommand &command)
{
    if (QThread::currentThread() != thread())
        qFatal("MockTaskService called outside its QObject thread");

    const QString taskId = command.taskId.isEmpty()
        ? QStringLiteral("mock-") + command.commandId : command.taskId;
    TaskEvent event;
    event.taskId = taskId;
    event.commandId = command.commandId;
    event.attemptId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.sequence = 1;

    if (command.type != TaskCommandType::Start) {
        event.type = TaskEventType::Failed;
        event.status = TaskStatus::Failed;
        event.errorCode = QStringLiteral("MOCK_UNSUPPORTED");
        event.message = QStringLiteral("Mock only accepts Start");
        emit taskEventOccurred(event);
        return;
    }

    TaskRecord record;
    record.taskId = taskId;
    record.attemptId = event.attemptId;
    record.caseId = command.caseId;
    record.inputSnapshot.snapshotId = command.inputSnapshotId;
    record.backendId = QStringLiteral("mock");
    record.solverId = QStringLiteral("none");
    record.status = TaskStatus::Preparing;
    record.createdAt = event.timestamp;
    records_.insert(taskId, record);

    event.type = TaskEventType::Prepared;
    event.status = TaskStatus::Preparing;
    event.message = QStringLiteral("MOCK prepared; no solver started");
    emit taskEventOccurred(event);

    QTimer::singleShot(0, this, [this, event]() mutable {
        TaskRecord &record = records_[event.taskId];
        record.status = TaskStatus::Finished;
        record.finishedAt = QDateTime::currentDateTimeUtc();
        event.sequence = 2;
        event.timestamp = record.finishedAt;
        event.type = TaskEventType::Finished;
        event.status = TaskStatus::Finished;
        event.message = QStringLiteral("MOCK roundtrip complete; no calculation performed");
        emit taskEventOccurred(event);
    });
}

} // namespace aerosim
