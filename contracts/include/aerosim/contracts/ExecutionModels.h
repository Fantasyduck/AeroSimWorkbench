#pragma once

#include "aerosim/contracts/ConfigurationModels.h"
#include "aerosim/contracts/MeshModels.h"
#include "aerosim/contracts/References.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace aerosim {

// Single execution state machine shared by tasks and attempts. Requested states
// are distinct from confirmed terminal states: Stop goes Running ->
// StopRequested -> Stopped and Cancel goes -> Cancelling -> Cancelled. Pause and
// Resume stay inside one attempt and never allocate a new one.
enum class ExecutionState {
    Unknown,
    Created,
    Queued,
    Preparing,
    Running,
    PauseRequested,
    Paused,
    StopRequested,
    Cancelling,
    Reconciling,
    Finished,
    Failed,
    Stopped,
    Cancelled
};

bool isTerminalExecutionState(ExecutionState state);
bool isNonTerminalExecutionState(ExecutionState state);
bool isPauseResumeExecutionState(ExecutionState state);

enum class TaskOrigin { Single, Batch };

// Immutable manifest of everything a run consumed. ParameterSnapshotRef alone is
// not enough: the manifest also pins the computation mesh, solver and backend,
// and is the authoritative list for restoring a historical calculation.
struct TaskInputManifest {
    ResourceRefV2 resource;            // must be typed ResourceType::TaskInputManifest
    TaskRef task;
    StateRef state;
    ParameterSnapshotRef parameterSnapshot;
    MeshRefV2 computationMesh;
    QString solverId;
    QString backendId;
    quint64 schemaVersion = 0;

    bool isValid() const;
};

// One execution try of a task. A retry keeps the TaskRef and allocates a new
// attemptId with a monotonic sequence starting at 1; the logs, monitors and
// errors of an attempt stay associated with that attempt.
struct TaskAttempt {
    AttemptRef ref;
    quint32 sequence = 0;
    ExecutionState state = ExecutionState::Created;
    QDateTime createdAt;
    QDateTime startedAt;
    QDateTime finishedAt;
    QString errorCode;
    QString errorMessage;
    QVector<ResourceRefV2> logRefs;
    QVector<ResourceRefV2> monitorRefs;
    QVector<ResourceRefV2> resultRefs;
    QVector<ResourceRefV2> checkpointRefs;

    bool isValid() const;
    bool isTerminal() const;
    bool hasError() const;
};

// A batch run is created when a plan is submitted and is immutable afterwards.
// taskRefs is derived membership only; TaskRef never carries batchRunId as part
// of its identity.
struct BatchRun {
    BatchRunRef ref;
    ResourceRefV2 executionSnapshot;   // typed ResourceType::BatchPlanSnapshot
    ExecutionState state = ExecutionState::Created;
    QDateTime createdAt;
    QDateTime finishedAt;
    QVector<TaskRef> taskRefs;
    QString errorCode;
    QString errorMessage;
    quint64 schemaVersion = 0;

    bool isValid() const;
    bool isTerminal() const;
    bool hasTask(const TaskRef &task) const;
};

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::TaskInputManifest)
Q_DECLARE_METATYPE(aerosim::TaskAttempt)
Q_DECLARE_METATYPE(aerosim::BatchRun)
