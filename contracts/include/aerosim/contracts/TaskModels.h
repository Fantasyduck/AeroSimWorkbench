#pragma once

#include "aerosim/contracts/References.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace aerosim {

enum class TaskCommandType { Start, Pause, Resume, Stop, Cancel };
enum class TaskEventType {
    Prepared, Started, StatusChanged, ProgressUpdated, ResidualUpdated,
    LogGenerated, Warning, Failed, Finished, ResultGenerated
};
enum class TaskStatus { Unknown, Preparing, Running, Paused, Finished, Failed, Cancelled };

struct TaskCommand {
    QString commandId;
    QString taskId;
    QString caseId;
    TaskCommandType type = TaskCommandType::Start;
    QString inputSnapshotId;
    QString batchPlanId;
    quint64 batchPlanVersion = 0;
};

struct TaskProgress {
    qint64 currentStep = 0;
    qint64 totalSteps = 0;
    double residual = 0.0;
};

struct TaskEvent {
    QString taskId;
    QString commandId;
    QString attemptId;
    TaskEventType type = TaskEventType::StatusChanged;
    TaskStatus status = TaskStatus::Unknown;
    quint64 sequence = 0;
    QDateTime timestamp;
    QString message;
    QString resourceRef;
    QString errorCode;
    TaskProgress progress;
};

// F owns persistence and status transitions; this is only the shared record shape.
struct TaskRecord {
    QString taskId;
    QString batchId;
    QString attemptId;
    QString caseId;
    ParameterSnapshotRef inputSnapshot;
    QString backendId;
    QString solverId;
    TaskStatus status = TaskStatus::Unknown;
    QDateTime createdAt;
    QDateTime startedAt;
    QDateTime finishedAt;
    QString logRef;
    QString monitorRef;
    QString resultRef;
    QString checkpointRef;
    QString errorCode;
    QString errorMessage;
};

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::TaskCommand)
Q_DECLARE_METATYPE(aerosim::TaskEvent)
