#include "TestSupport.h"

#include "aerosim/contracts/EnumTokens.h"

#include <type_traits>

namespace {

aerosim::ResourceRefV2 verifiedResource(const QString &resourceId,
                                        aerosim::ResourceType type)
{
    aerosim::ResourceRefV2 resource;
    resource.projectId = QStringLiteral("project-a");
    resource.resourceId = resourceId;
    resource.revision = 1;
    resource.type = type;
    resource.location = aerosim::ResourceLocation::projectRelative(
        QStringLiteral("runtime/input.json"));
    resource.fingerprint.status = aerosim::FingerprintStatus::Verified;
    resource.fingerprint.algorithm = QStringLiteral("sha256");
    resource.fingerprint.digest = QStringLiteral("0123456789abcdef");
    return resource;
}

aerosim::StateRef pinnedState()
{
    aerosim::StateRef state;
    state.projectId = QStringLiteral("project-a");
    state.stateId = QStringLiteral("state-a");
    state.revision = 4;
    return state;
}

aerosim::BatchPlanRef pinnedPlan()
{
    aerosim::BatchPlanRef plan;
    plan.projectId = QStringLiteral("project-a");
    plan.planId = QStringLiteral("plan-a");
    plan.revision = 2;
    return plan;
}

aerosim::TaskRef taskRef(const QString &batchRunId = QString())
{
    aerosim::TaskRef task;
    task.projectId = QStringLiteral("project-a");
    task.taskId = QStringLiteral("task-a");
    task.batchRunId = batchRunId;
    return task;
}

aerosim::TaskRef otherTaskRef()
{
    aerosim::TaskRef task;
    task.projectId = QStringLiteral("project-a");
    task.taskId = QStringLiteral("task-b");
    return task;
}

aerosim::TaskInputManifest manifest(const aerosim::TaskRef &task)
{
    aerosim::TaskInputManifest input;
    input.resource = verifiedResource(QStringLiteral("manifest-a"),
                                      aerosim::ResourceType::TaskInputManifest);
    input.task = task;
    input.state = pinnedState();
    input.parameterSnapshot.snapshotId = QStringLiteral("snapshot-1");
    input.parameterSnapshot.revision = 2;
    input.parameterSnapshot.scope.projectId = QStringLiteral("project-a");
    input.parameterSnapshot.scope.caseId = input.state.stateId;
    input.computationMesh.projectId = QStringLiteral("project-a");
    input.computationMesh.meshId = QStringLiteral("mesh-a");
    input.computationMesh.revision = 3;
    input.solverId = QStringLiteral("structured-density-based");
    input.backendId = QStringLiteral("local-process");
    input.schemaVersion = 1;
    return input;
}

aerosim::TaskAttempt attempt(const aerosim::TaskRef &task, quint32 sequence,
                            const QString &attemptId,
                            aerosim::ExecutionState state)
{
    aerosim::TaskAttempt record;
    record.ref.task = task;
    record.ref.attemptId = attemptId;
    record.sequence = sequence;
    record.state = state;
    return record;
}

aerosim::TaskSnapshot singleTask(quint32 attemptCount,
                                aerosim::ExecutionState executionState)
{
    const aerosim::TaskRef task = taskRef();
    aerosim::TaskSnapshot snapshot;
    snapshot.ref = task;
    snapshot.origin = aerosim::TaskOrigin::Single;
    snapshot.state = pinnedState();
    snapshot.inputManifest = manifest(task);
    snapshot.executionState = executionState;
    snapshot.schemaVersion = 1;
    for (quint32 index = 0; index < attemptCount; ++index) {
        const QString attemptId = QStringLiteral("attempt-%1").arg(index + 1);
        snapshot.attempts.append(attempt(task, index + 1, attemptId,
            index + 1 == attemptCount ? executionState
                                      : aerosim::ExecutionState::Failed));
    }
    if (!snapshot.attempts.isEmpty())
        snapshot.currentAttemptId = snapshot.attempts.last().ref.attemptId;
    return snapshot;
}

void checkTerminalClassification(aerosim_test::TestContext &context)
{
    AEROSIM_CHECK(context, aerosim::isTerminalExecutionState(aerosim::ExecutionState::Finished));
    AEROSIM_CHECK(context, aerosim::isTerminalExecutionState(aerosim::ExecutionState::Failed));
    AEROSIM_CHECK(context, aerosim::isTerminalExecutionState(aerosim::ExecutionState::Stopped));
    AEROSIM_CHECK(context, aerosim::isTerminalExecutionState(aerosim::ExecutionState::Cancelled));

    const aerosim::ExecutionState nonTerminal[] = {
        aerosim::ExecutionState::Created,
        aerosim::ExecutionState::Queued,
        aerosim::ExecutionState::Preparing,
        aerosim::ExecutionState::Running,
        aerosim::ExecutionState::PauseRequested,
        aerosim::ExecutionState::Paused,
        aerosim::ExecutionState::StopRequested,
        aerosim::ExecutionState::Cancelling,
        aerosim::ExecutionState::Reconciling
    };
    for (aerosim::ExecutionState state : nonTerminal) {
        AEROSIM_CHECK(context, !aerosim::isTerminalExecutionState(state));
        AEROSIM_CHECK(context, aerosim::isNonTerminalExecutionState(state));
    }

    // Unknown is neither terminal nor a normal new-task state.
    AEROSIM_CHECK(context, !aerosim::isTerminalExecutionState(aerosim::ExecutionState::Unknown));
    AEROSIM_CHECK(context, !aerosim::isNonTerminalExecutionState(aerosim::ExecutionState::Unknown));
}

void checkCommandMapping(aerosim_test::TestContext &context)
{
    using aerosim::TaskCommandType;
    using aerosim::ExecutionState;

    // Requested states stay distinct for Stop and Cancel.
    AEROSIM_CHECK(context,
        aerosim::requestedStateFor(TaskCommandType::Stop) == ExecutionState::StopRequested);
    AEROSIM_CHECK(context,
        aerosim::requestedStateFor(TaskCommandType::Cancel) == ExecutionState::Cancelling);
    AEROSIM_CHECK(context,
        aerosim::requestedStateFor(TaskCommandType::Pause) == ExecutionState::PauseRequested);
    AEROSIM_CHECK(context,
        aerosim::requestedStateFor(TaskCommandType::Resume) == ExecutionState::Running);
    AEROSIM_CHECK(context,
        aerosim::requestedStateFor(TaskCommandType::Start) == ExecutionState::Queued);

    // Only backend-confirmed commands have a terminal state.
    AEROSIM_CHECK(context,
        aerosim::confirmedTerminalStateFor(TaskCommandType::Stop) == ExecutionState::Stopped);
    AEROSIM_CHECK(context,
        aerosim::confirmedTerminalStateFor(TaskCommandType::Cancel) == ExecutionState::Cancelled);
    AEROSIM_CHECK(context,
        aerosim::confirmedTerminalStateFor(TaskCommandType::Pause) == ExecutionState::Unknown);

    AEROSIM_CHECK(context,
        aerosim::canAcceptCommand(ExecutionState::Running, TaskCommandType::Pause));
    AEROSIM_CHECK(context,
        !aerosim::canAcceptCommand(ExecutionState::Paused, TaskCommandType::Pause));
    AEROSIM_CHECK(context,
        aerosim::canAcceptCommand(ExecutionState::Paused, TaskCommandType::Resume));
    AEROSIM_CHECK(context,
        aerosim::canAcceptCommand(ExecutionState::Queued, TaskCommandType::Cancel));
    AEROSIM_CHECK(context,
        !aerosim::canAcceptCommand(ExecutionState::StopRequested, TaskCommandType::Stop));
    AEROSIM_CHECK(context,
        !aerosim::canAcceptCommand(ExecutionState::Finished, TaskCommandType::Start));
    AEROSIM_CHECK(context,
        !aerosim::canAcceptCommand(ExecutionState::Unknown, TaskCommandType::Start));

    // Pause/Resume stays inside one attempt and never allocates a new attempt.
    AEROSIM_CHECK(context, aerosim::isPauseResumeExecutionState(ExecutionState::Paused));
    AEROSIM_CHECK(context, !aerosim::isPauseResumeExecutionState(ExecutionState::StopRequested));
}

void checkInputManifest(aerosim_test::TestContext &context)
{
    aerosim::TaskInputManifest input;
    AEROSIM_CHECK(context, !input.isValid());

    input = manifest(taskRef());
    AEROSIM_CHECK(context, input.isValid());

    // The manifest must be a typed task input manifest resource.
    input.resource.type = aerosim::ResourceType::BatchPlanSnapshot;
    AEROSIM_CHECK(context, !input.isValid());
    input.resource.type = aerosim::ResourceType::TaskInputManifest;

    // Unspecified mesh revision or parameter snapshot is not a usable input.
    input.computationMesh.revision = 0;
    AEROSIM_CHECK(context, !input.isValid());
    input.computationMesh.revision = 3;
    input.parameterSnapshot.revision = 0;
    AEROSIM_CHECK(context, !input.isValid());
    input.parameterSnapshot.revision = 2;

    // A manifest never belongs to a task from a different project.
    input.task.projectId = QStringLiteral("project-b");
    AEROSIM_CHECK(context, !input.isValid());
}

void checkAttempts(aerosim_test::TestContext &context)
{
    const aerosim::TaskRef task = taskRef();
    aerosim::TaskAttempt record = attempt(task, 1, QStringLiteral("attempt-1"),
                                         aerosim::ExecutionState::Running);
    AEROSIM_CHECK(context, record.isValid());
    AEROSIM_CHECK(context, !record.isTerminal());
    AEROSIM_CHECK(context, !record.hasError());

    record.state = aerosim::ExecutionState::Failed;
    record.errorCode = QStringLiteral("solver.diverge");
    AEROSIM_CHECK(context, record.isTerminal());
    AEROSIM_CHECK(context, record.hasError());

    // A retry keeps the task identity and only changes the attempt identity.
    const aerosim::TaskAttempt retry = attempt(task, 2, QStringLiteral("attempt-2"),
                                              aerosim::ExecutionState::Running);
    AEROSIM_CHECK(context, retry.ref.task.sameIdentity(record.ref.task));
    AEROSIM_CHECK(context, retry.ref.task.sameVersion(record.ref.task));
    AEROSIM_CHECK(context, !retry.ref.sameIdentity(record.ref));

    // Sequence 0 is a malformed attempt, not the first try.
    const aerosim::TaskAttempt unsequenced =
        attempt(task, 0, QStringLiteral("attempt-x"),
                aerosim::ExecutionState::Created);
    AEROSIM_CHECK(context, !unsequenced.isValid());
}

void checkBatchRun(aerosim_test::TestContext &context)
{
    const aerosim::TaskRef task = taskRef(QStringLiteral("run-a"));
    aerosim::BatchRun run;
    AEROSIM_CHECK(context, !run.isValid());

    run.ref.projectId = QStringLiteral("project-a");
    run.ref.batchRunId = QStringLiteral("run-a");
    run.ref.sourcePlan = pinnedPlan();
    run.executionSnapshot = verifiedResource(QStringLiteral("plan-snapshot"),
        aerosim::ResourceType::BatchPlanSnapshot);
    run.state = aerosim::ExecutionState::Running;
    run.taskRefs.append(task);
    AEROSIM_CHECK(context, run.isValid());
    // batchRunId is metadata and must not participate in Task identity.
    AEROSIM_CHECK(context, run.hasTask(taskRef(QStringLiteral("other-run"))));
    AEROSIM_CHECK(context, !run.hasTask(otherTaskRef()));
    AEROSIM_CHECK(context, !run.isTerminal());

    // The execution snapshot must be a batch plan snapshot resource.
    run.executionSnapshot.type = aerosim::ResourceType::Result;
    AEROSIM_CHECK(context, !run.isValid());
    run.executionSnapshot.type = aerosim::ResourceType::BatchPlanSnapshot;

    // A task from another project must not belong to this run.
    aerosim::TaskRef foreign = taskRef(QStringLiteral("run-a"));
    foreign.projectId = QStringLiteral("project-b");
    run.taskRefs.append(foreign);
    AEROSIM_CHECK(context, !run.isValid());
    run.taskRefs.removeLast();

    // sourcePlan is provenance and does not change BatchRun identity.
    aerosim::BatchRun newerPlan = run;
    newerPlan.ref.sourcePlan.revision = 5;
    AEROSIM_CHECK(context, run.ref.sameIdentity(newerPlan.ref));
    AEROSIM_CHECK(context, run.ref.sameVersion(newerPlan.ref));

    run.state = aerosim::ExecutionState::Finished;
    AEROSIM_CHECK(context, run.isTerminal());
}

void checkSingleTaskSnapshot(aerosim_test::TestContext &context)
{
    aerosim::TaskSnapshot empty;
    AEROSIM_CHECK(context, !empty.isValid());
    AEROSIM_CHECK(context, !empty.isBatchTask());

    aerosim::TaskSnapshot snapshot = singleTask(1, aerosim::ExecutionState::Running);
    AEROSIM_CHECK(context, snapshot.isValid());
    AEROSIM_CHECK(context, !snapshot.isBatchTask());
    AEROSIM_CHECK(context, snapshot.hasUniqueAttemptIds());
    AEROSIM_CHECK(context, snapshot.currentAttempt() != nullptr);
    AEROSIM_CHECK(context, snapshot.nextAttemptSequence() == 2);
    AEROSIM_CHECK(context, snapshot.canPauseResume());
    AEROSIM_CHECK(context, !snapshot.canRetry());
    AEROSIM_CHECK(context, snapshot.findAttempt(QStringLiteral("attempt-9")) == nullptr);

    // A state revision mismatch with the pinned manifest is invalid.
    snapshot.state.revision = snapshot.state.revision + 1;
    AEROSIM_CHECK(context, !snapshot.isValid());
    snapshot.state = pinnedState();
    AEROSIM_CHECK(context, snapshot.isValid());

    // A single task must not claim a batch run.
    snapshot.batchRun.projectId = QStringLiteral("project-a");
    snapshot.batchRun.batchRunId = QStringLiteral("run-a");
    snapshot.batchRun.sourcePlan = pinnedPlan();
    AEROSIM_CHECK(context, !snapshot.isValid());
    snapshot.batchRun = aerosim::BatchRunRef();

    // A current attempt id that is not in the attempt list is inconsistent.
    snapshot.currentAttemptId = QStringLiteral("attempt-missing");
    AEROSIM_CHECK(context, !snapshot.isValid());
    snapshot.currentAttemptId = QStringLiteral("attempt-1");
    AEROSIM_CHECK(context, snapshot.isValid());

    // Duplicate attempt ids are rejected.
    aerosim::TaskSnapshot duplicated = singleTask(2, aerosim::ExecutionState::Running);
    duplicated.attempts[1].ref.attemptId = duplicated.attempts[0].ref.attemptId;
    duplicated.currentAttemptId = duplicated.attempts[0].ref.attemptId;
    AEROSIM_CHECK(context, !duplicated.hasUniqueAttemptIds());
    AEROSIM_CHECK(context, !duplicated.isValid());
}

void checkRetryAndTerminalSemantics(aerosim_test::TestContext &context)
{
    // Retrying a failed task reuses the taskId with a new attempt.
    aerosim::TaskSnapshot failed = singleTask(1, aerosim::ExecutionState::Failed);
    AEROSIM_CHECK(context, failed.isTerminal());
    AEROSIM_CHECK(context, failed.canRetry());
    AEROSIM_CHECK(context, !failed.canPauseResume());
    failed.attempts.append(attempt(failed.ref, failed.nextAttemptSequence(),
                                   QStringLiteral("attempt-2"),
                                   aerosim::ExecutionState::Running));
    failed.currentAttemptId = QStringLiteral("attempt-2");
    AEROSIM_CHECK(context, failed.isValid());
    AEROSIM_CHECK(context, failed.nextAttemptSequence() == 3);

    // Stopped and Cancelled are distinct terminal states and both retryable.
    aerosim::TaskSnapshot stopped = singleTask(1, aerosim::ExecutionState::Stopped);
    aerosim::TaskSnapshot cancelled = singleTask(1, aerosim::ExecutionState::Cancelled);
    AEROSIM_CHECK(context, stopped.canRetry());
    AEROSIM_CHECK(context, cancelled.canRetry());
    AEROSIM_CHECK(context, stopped.executionState != cancelled.executionState);

    // A finished task needs a new taskId, not a retry attempt.
    aerosim::TaskSnapshot finished = singleTask(1, aerosim::ExecutionState::Finished);
    AEROSIM_CHECK(context, finished.isTerminal());
    AEROSIM_CHECK(context, !finished.canRetry());

    // A running task is not retryable and requested states are non-terminal.
    aerosim::TaskSnapshot running = singleTask(1, aerosim::ExecutionState::Running);
    AEROSIM_CHECK(context, !running.canRetry());
    AEROSIM_CHECK(context,
        !aerosim::isTerminalExecutionState(
            aerosim::requestedStateFor(aerosim::TaskCommandType::Stop)));
}

void checkBatchTaskSnapshot(aerosim_test::TestContext &context)
{
    aerosim::TaskSnapshot snapshot = singleTask(1, aerosim::ExecutionState::Running);
    snapshot.origin = aerosim::TaskOrigin::Batch;
    // A batch task must reference a valid BatchRun.
    AEROSIM_CHECK(context, !snapshot.isValid());

    snapshot.ref.batchRunId = QStringLiteral("run-a");
    snapshot.batchRun.projectId = QStringLiteral("project-a");
    snapshot.batchRun.batchRunId = QStringLiteral("run-a");
    snapshot.batchRun.sourcePlan = pinnedPlan();
    AEROSIM_CHECK(context, snapshot.isValid());
    AEROSIM_CHECK(context, snapshot.isBatchTask());

    // A batch run from another project cannot own this task.
    snapshot.batchRun.projectId = QStringLiteral("project-b");
    AEROSIM_CHECK(context, !snapshot.isValid());
}

void checkTokens(aerosim_test::TestContext &context)
{
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::ExecutionState::StopRequested)
            == QStringLiteral("stop_requested"));
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::ExecutionState::Cancelling)
            == QStringLiteral("cancelling"));
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::ExecutionState::Stopped) == QStringLiteral("stopped"));
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::ExecutionState::Cancelled) == QStringLiteral("cancelled"));
    AEROSIM_CHECK(context,
        aerosim::executionStateFromToken(QStringLiteral("reconciling")).value()
            == aerosim::ExecutionState::Reconciling);
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::TaskOrigin::Batch) == QStringLiteral("batch"));
    AEROSIM_CHECK(context,
        aerosim::taskOriginFromToken(QStringLiteral("single")).value()
            == aerosim::TaskOrigin::Single);
    AEROSIM_CHECK(context,
        aerosim::executionStateFromToken(QStringLiteral("nope")).status()
            == aerosim::AccessStatus::InvalidArgument);
}

} // namespace

int main()
{
    aerosim_test::TestContext context;
    checkTerminalClassification(context);
    checkCommandMapping(context);
    checkInputManifest(context);
    checkAttempts(context);
    checkBatchRun(context);
    checkSingleTaskSnapshot(context);
    checkRetryAndTerminalSemantics(context);
    checkBatchTaskSnapshot(context);
    checkTokens(context);
    return context.result();
}
