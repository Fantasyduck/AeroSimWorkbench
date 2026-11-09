#include "aerosim/contracts/ExecutionModels.h"

namespace aerosim {

bool isTerminalExecutionState(ExecutionState state)
{
    switch (state) {
    case ExecutionState::Finished:
    case ExecutionState::Failed:
    case ExecutionState::Stopped:
    case ExecutionState::Cancelled:
        return true;
    case ExecutionState::Unknown:
    case ExecutionState::Created:
    case ExecutionState::Queued:
    case ExecutionState::Preparing:
    case ExecutionState::Running:
    case ExecutionState::PauseRequested:
    case ExecutionState::Paused:
    case ExecutionState::StopRequested:
    case ExecutionState::Cancelling:
    case ExecutionState::Reconciling:
        return false;
    }
    return false;
}

bool isNonTerminalExecutionState(ExecutionState state)
{
    if (state == ExecutionState::Unknown)
        return false;
    return !isTerminalExecutionState(state);
}

bool isPauseResumeExecutionState(ExecutionState state)
{
    return state == ExecutionState::Running || state == ExecutionState::Paused
        || state == ExecutionState::PauseRequested;
}

bool TaskInputManifest::isValid() const
{
    if (!resource.isValid() || resource.type != ResourceType::TaskInputManifest)
        return false;
    if (!task.isValid() || !state.isValid())
        return false;
    if (resource.projectId != task.projectId || state.projectId != task.projectId)
        return false;
    if (!computationMesh.isValid() || computationMesh.projectId != task.projectId)
        return false;
    if (parameterSnapshot.snapshotId.isEmpty() || parameterSnapshot.revision == 0)
        return false;
    return !solverId.isEmpty();
}

bool TaskAttempt::isValid() const
{
    return ref.isValid() && sequence > 0;
}

bool TaskAttempt::isTerminal() const
{
    return isTerminalExecutionState(state);
}

bool TaskAttempt::hasError() const
{
    return !errorCode.isEmpty() || !errorMessage.isEmpty();
}

bool BatchRun::isValid() const
{
    if (!ref.isValid())
        return false;
    if (!executionSnapshot.isValid()
        || executionSnapshot.type != ResourceType::BatchPlanSnapshot)
        return false;
    if (executionSnapshot.projectId != ref.projectId)
        return false;
    for (const TaskRef &task : taskRefs) {
        if (!task.isValid() || task.projectId != ref.projectId)
            return false;
    }
    return true;
}

bool BatchRun::isTerminal() const
{
    return isTerminalExecutionState(state);
}

bool BatchRun::hasTask(const TaskRef &task) const
{
    for (const TaskRef &candidate : taskRefs) {
        if (candidate.sameIdentity(task))
            return true;
    }
    return false;
}

} // namespace aerosim
