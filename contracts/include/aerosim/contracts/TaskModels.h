#pragma once

#include "aerosim/contracts/ExecutionModels.h"
#include "aerosim/contracts/MeshModels.h"
#include "aerosim/contracts/References.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>
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

// Command-to-state mapping. Requested and confirmed states are distinct so Pause,
// Resume, Stop and Cancel can never collapse into one status; the runtime only
// publishes a terminal state after the execution backend confirms it.
ExecutionState requestedStateFor(TaskCommandType command);
ExecutionState confirmedTerminalStateFor(TaskCommandType command);
bool canAcceptCommand(ExecutionState state, TaskCommandType command);

// Read-only aggregate view of one task. F/runtime owns persistence and status
// transitions; this is the shared shape for queries, restoration and reports.
struct TaskSnapshot {
    TaskRef ref;
    TaskOrigin origin = TaskOrigin::Single;
    StateRef state;                    // the exact State revision consumed
    BatchRunRef batchRun;              // used only when origin is Batch
    QString currentAttemptId;
    TaskInputManifest inputManifest;
    QVector<TaskAttempt> attempts;
    ExecutionState executionState = ExecutionState::Created;
    QDateTime createdAt;
    QDateTime startedAt;
    QDateTime finishedAt;
    QVector<ResourceRefV2> logRefs;
    QVector<ResourceRefV2> monitorRefs;
    QVector<ResourceRefV2> resultRefs;
    QVector<ResourceRefV2> checkpointRefs;
    QString errorCode;
    QString errorMessage;
    quint64 recordRevision = 0;
    quint64 schemaVersion = 0;

    bool isValid() const;
    bool isTerminal() const;
    bool isBatchTask() const;
    bool hasUniqueAttemptIds() const;
    quint32 nextAttemptSequence() const;
    const TaskAttempt *currentAttempt() const;
    const TaskAttempt *findAttempt(const QString &attemptId) const;

    // A finished task needs a new taskId; only Failed/Stopped/Cancelled retries
    // with unchanged input may reuse this taskId.
    bool canRetry() const;
    bool canPauseResume() const;
};

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::TaskCommand)
Q_DECLARE_METATYPE(aerosim::TaskEvent)
Q_DECLARE_METATYPE(aerosim::TaskSnapshot)
