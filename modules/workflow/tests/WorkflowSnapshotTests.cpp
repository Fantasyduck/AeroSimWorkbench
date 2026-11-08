#include "WorkflowTestSupport.h"

#include "aerosim/workflow/WorkflowService.h"

#include <QCoreApplication>

namespace {

aerosim::ScopeRef scope()
{
    aerosim::ScopeRef value;
    value.projectId = QStringLiteral("project");
    value.caseId = QStringLiteral("case");
    value.objectId = QStringLiteral("state");
    return value;
}

aerosim::workflow::WorkflowDefinition definition()
{
    aerosim::workflow::WorkflowDefinition value;
    value.definitionVersion = QStringLiteral("workflow-v2");
    aerosim::workflow::WorkflowStepDefinition step;
    step.stepId = QStringLiteral("configured");
    step.applicabilityRuleId = QStringLiteral("always");
    value.steps.append(step);
    return value;
}

aerosim::workflow::ValidationInputStamp stamp(quint64 revision)
{
    aerosim::workflow::ValidationInputStamp value;
    value.scope = scope();
    value.definitionVersion = QStringLiteral("workflow-v2");
    value.parameter.snapshotId = QStringLiteral("parameters");
    value.parameter.scope = value.scope;
    value.parameter.revision = revision;
    return value;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application);
    aerosim_workflow_test::TestContext context;

    aerosim::workflow::WorkflowService source;
    AEROSIM_WORKFLOW_CHECK(context,
        source.setDefinition(definition()).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        source.setCurrentInput(scope(), QStringLiteral("configured"),
                               stamp(1)).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        source.saveEditing(scope(), QStringLiteral("configured")).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        source.acceptValidation(scope(), QStringLiteral("configured"),
            aerosim::workflow::ValidationOutcome::Passed, stamp(1)).ok());
    const auto exported = source.snapshot(scope());
    AEROSIM_WORKFLOW_CHECK(context, exported.ok());

    // The value shape remains storage neutral and stable under repeated
    // facts-to-snapshot round trips. capturedAt is metadata, not fact identity.
    aerosim::workflow::WorkflowFactsSnapshot roundTrip = exported.value();
    for (int index = 0; index < 100; ++index) {
        const auto validated =
            aerosim::workflow::validateWorkflowFactsSnapshot(roundTrip);
        AEROSIM_WORKFLOW_CHECK(context, validated.ok());
        const auto made = aerosim::workflow::makeWorkflowFactsSnapshot(
            aerosim::workflow::workflowFactsFromSnapshot(validated.value()),
            roundTrip.capturedAt.addMSecs(index + 1));
        AEROSIM_WORKFLOW_CHECK(context, made.ok());
        AEROSIM_WORKFLOW_CHECK(context,
            aerosim::workflow::workflowSnapshotFactsEqual(
                roundTrip, made.value()));
        roundTrip = made.value();
    }

    aerosim::workflow::WorkflowService restored;
    AEROSIM_WORKFLOW_CHECK(context,
        restored.setDefinition(definition()).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        restored.setCurrentInput(scope(), QStringLiteral("configured"),
                                 stamp(1)).ok());
    int changed = 0;
    QObject::connect(&restored,
        &aerosim::IWorkflowService::workflowViewV2Changed,
        [&changed](const aerosim::WorkflowViewV2 &) { ++changed; });
    AEROSIM_WORKFLOW_CHECK(context,
        restored.restoreSnapshot(exported.value()).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        restored.facts(scope()).value().factRevision
            == exported.value().factRevision);
    AEROSIM_WORKFLOW_CHECK(context,
        restored.viewV2(scope()).value().steps.first().validation
            == aerosim::ValidationState::Valid);

    // Same revision and same facts are idempotent, including a different
    // capture time, and therefore emit no new view signal.
    const int afterFirstRestore = changed;
    aerosim::workflow::WorkflowFactsSnapshot identical = exported.value();
    identical.capturedAt = identical.capturedAt.addSecs(1);
    AEROSIM_WORKFLOW_CHECK(context,
        restored.restoreSnapshot(identical).ok());
    AEROSIM_WORKFLOW_CHECK(context, changed == afterFirstRestore);

    // Lower revisions and same-revision divergent facts are conflicts and
    // leave the stored facts unchanged.
    const auto beforeConflict = restored.snapshot(scope()).value();
    aerosim::workflow::WorkflowFactsSnapshot lower = exported.value();
    lower.factRevision -= 1;
    lower.steps.first().factRevision = lower.factRevision;
    const auto lowerResult = restored.restoreSnapshot(lower);
    AEROSIM_WORKFLOW_CHECK(context,
        !lowerResult.ok()
            && lowerResult.status() == aerosim::AccessStatus::Conflict);
    aerosim::workflow::WorkflowFactsSnapshot divergent = exported.value();
    divergent.steps.first().committedCompletion =
        aerosim::CompletionState::InProgress;
    const auto divergentResult = restored.restoreSnapshot(divergent);
    AEROSIM_WORKFLOW_CHECK(context,
        !divergentResult.ok()
            && divergentResult.status() == aerosim::AccessStatus::Conflict);
    AEROSIM_WORKFLOW_CHECK(context,
        aerosim::workflow::workflowSnapshotFactsEqual(
            beforeConflict, restored.snapshot(scope()).value()));

    // A higher revision is accepted atomically. Runtime drafts and pending
    // validation requests are cleared, while the next fact write continues
    // from the restored maximum revision.
    AEROSIM_WORKFLOW_CHECK(context,
        restored.beginEditing(scope(), QStringLiteral("configured"),
                              QStringLiteral("draft")).ok());
    const auto pending = restored.beginValidation(
        scope(), QStringLiteral("configured"), QStringLiteral("pending"));
    AEROSIM_WORKFLOW_CHECK(context, pending.ok());
    aerosim::workflow::WorkflowFactsSnapshot higher = exported.value();
    higher.factRevision = 20;
    higher.steps.first().factRevision = 20;
    AEROSIM_WORKFLOW_CHECK(context,
        restored.restoreSnapshot(higher).value() == 20);
    AEROSIM_WORKFLOW_CHECK(context,
        restored.viewV2(scope()).value().steps.first().completion
            == aerosim::CompletionState::Completed);
    aerosim::workflow::ValidationCompletion oldPending;
    oldPending.requestId = QStringLiteral("pending");
    oldPending.scope = scope();
    oldPending.stepId = QStringLiteral("configured");
    oldPending.generation = pending.value().generation;
    oldPending.inputStamp = stamp(1);
    oldPending.outcome = aerosim::workflow::ValidationOutcome::Failed;
    oldPending.completedAt = QDateTime::currentDateTimeUtc();
    restored.onValidationCompleted(oldPending);
    AEROSIM_WORKFLOW_CHECK(context,
        restored.facts(scope()).value().factRevision == 20);
    AEROSIM_WORKFLOW_CHECK(context,
        restored.saveEditing(scope(), QStringLiteral("configured")).value()
            == 21);

    // Derived validation is recalculated from the target service's current
    // inputs instead of trusting the old view that accompanied the facts.
    aerosim::workflow::WorkflowService staleTarget;
    AEROSIM_WORKFLOW_CHECK(context,
        staleTarget.setDefinition(definition()).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        staleTarget.setCurrentInput(scope(), QStringLiteral("configured"),
                                    stamp(2)).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        staleTarget.restoreSnapshot(exported.value()).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        staleTarget.viewV2(scope()).value().steps.first().validation
            == aerosim::ValidationState::Stale);

    // Invalid snapshots are rejected before mutation.
    const auto beforeInvalid = staleTarget.snapshot(scope()).value();
    aerosim::workflow::WorkflowFactsSnapshot invalid = exported.value();
    invalid.steps.append(invalid.steps.first());
    const auto invalidResult = staleTarget.restoreSnapshot(invalid);
    AEROSIM_WORKFLOW_CHECK(context,
        !invalidResult.ok()
            && invalidResult.status()
                == aerosim::AccessStatus::InvalidArgument);
    AEROSIM_WORKFLOW_CHECK(context,
        aerosim::workflow::workflowSnapshotFactsEqual(
            beforeInvalid, staleTarget.snapshot(scope()).value()));

    // The active view follows the current definition: facts for removed steps
    // remain historical snapshot facts, while newly defined steps start empty.
    aerosim::workflow::WorkflowDefinition changedDefinition;
    changedDefinition.definitionVersion = QStringLiteral("workflow-v2-next");
    aerosim::workflow::WorkflowStepDefinition newStep;
    newStep.stepId = QStringLiteral("new-step");
    newStep.applicabilityRuleId = QStringLiteral("always");
    changedDefinition.steps.append(newStep);
    aerosim::workflow::WorkflowService changedTarget;
    AEROSIM_WORKFLOW_CHECK(context,
        changedTarget.setDefinition(changedDefinition).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        changedTarget.restoreSnapshot(exported.value()).ok());
    const aerosim::WorkflowViewV2 changedView =
        changedTarget.viewV2(scope()).value();
    AEROSIM_WORKFLOW_CHECK(context, changedView.steps.size() == 1);
    AEROSIM_WORKFLOW_CHECK(context,
        changedView.steps.first().stepId == QStringLiteral("new-step"));
    AEROSIM_WORKFLOW_CHECK(context,
        changedView.steps.first().completion
            == aerosim::CompletionState::NotStarted);

    return context.result();
}
