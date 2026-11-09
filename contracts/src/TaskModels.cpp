#include "aerosim/contracts/TaskModels.h"

namespace aerosim {

ExecutionState requestedStateFor(TaskCommandType command)
{
    switch (command) {
    case TaskCommandType::Start:
        return ExecutionState::Queued;
    case TaskCommandType::Pause:
        return ExecutionState::PauseRequested;
    case TaskCommandType::Resume:
        return ExecutionState::Running;
    case TaskCommandType::Stop:
        return ExecutionState::StopRequested;
    case TaskCommandType::Cancel:
        return ExecutionState::Cancelling;
    }
    return ExecutionState::Unknown;
}

ExecutionState confirmedTerminalStateFor(TaskCommandType command)
{
    switch (command) {
    case TaskCommandType::Stop:
        return ExecutionState::Stopped;
    case TaskCommandType::Cancel:
        return ExecutionState::Cancelled;
    case TaskCommandType::Start:
    case TaskCommandType::Pause:
    case TaskCommandType::Resume:
        return ExecutionState::Unknown;
    }
    return ExecutionState::Unknown;
}

bool canAcceptCommand(ExecutionState state, TaskCommandType command)
{
    if (isTerminalExecutionState(state) || state == ExecutionState::Unknown)
        return false;
    switch (command) {
    case TaskCommandType::Start:
        return state == ExecutionState::Created || state == ExecutionState::Reconciling;
    case TaskCommandType::Pause:
        return state == ExecutionState::Running;
    case TaskCommandType::Resume:
        return state == ExecutionState::Paused;
    case TaskCommandType::Stop:
        return state == ExecutionState::Running || state == ExecutionState::Paused
            || state == ExecutionState::Preparing;
    case TaskCommandType::Cancel:
        return state == ExecutionState::Queued || state == ExecutionState::Preparing
            || state == ExecutionState::Running || state == ExecutionState::Paused
            || state == ExecutionState::PauseRequested;
    }
    return false;
}

bool TaskSnapshot::isValid() const
{
    if (!ref.isValid() || !state.isValid())
        return false;
    if (state.projectId != ref.projectId)
        return false;
    if (!inputManifest.isValid() || !inputManifest.task.sameIdentity(ref))
        return false;
    // The task pins the exact State revision it consumed.
    if (!inputManifest.state.sameVersion(state))
        return false;
    if (origin == TaskOrigin::Batch) {
        if (!batchRun.isValid() || batchRun.projectId != ref.projectId)
            return false;
    } else if (!batchRun.batchRunId.isEmpty()) {
        // A single task must not pretend to belong to a batch run.
        return false;
    }
    if (!hasUniqueAttemptIds())
        return false;
    for (const TaskAttempt &attempt : attempts) {
        if (!attempt.isValid() || !attempt.ref.task.sameIdentity(ref))
            return false;
    }
    if (!currentAttemptId.isEmpty() && findAttempt(currentAttemptId) == nullptr)
        return false;
    return true;
}

bool TaskSnapshot::isTerminal() const
{
    return isTerminalExecutionState(executionState);
}

bool TaskSnapshot::isBatchTask() const
{
    return origin == TaskOrigin::Batch;
}

bool TaskSnapshot::hasUniqueAttemptIds() const
{
    for (int i = 0; i < attempts.size(); ++i) {
        for (int j = i + 1; j < attempts.size(); ++j) {
            if (attempts.at(i).ref.attemptId == attempts.at(j).ref.attemptId)
                return false;
        }
    }
    return true;
}

quint32 TaskSnapshot::nextAttemptSequence() const
{
    quint32 next = 0;
    for (const TaskAttempt &attempt : attempts) {
        if (attempt.sequence > next)
            next = attempt.sequence;
    }
    return next + 1;
}

const TaskAttempt *TaskSnapshot::currentAttempt() const
{
    if (currentAttemptId.isEmpty())
        return nullptr;
    return findAttempt(currentAttemptId);
}

const TaskAttempt *TaskSnapshot::findAttempt(const QString &attemptId) const
{
    for (const TaskAttempt &attempt : attempts) {
        if (attempt.ref.attemptId == attemptId)
            return &attempt;
    }
    return nullptr;
}

bool TaskSnapshot::canRetry() const
{
    if (!isTerminal())
        return false;
    // Finished needs a new taskId because retrying finished work changes input.
    return executionState == ExecutionState::Failed
        || executionState == ExecutionState::Stopped
        || executionState == ExecutionState::Cancelled;
}

bool TaskSnapshot::canPauseResume() const
{
    return executionState == ExecutionState::Running
        || executionState == ExecutionState::Paused;
}

} // namespace aerosim
