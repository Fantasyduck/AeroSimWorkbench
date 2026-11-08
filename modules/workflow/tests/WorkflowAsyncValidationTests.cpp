#include "WorkflowTestSupport.h"

#include "aerosim/workflow/ValidationCoordinator.h"
#include "aerosim/workflow/WorkflowService.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QMetaObject>
#include <QMetaType>

namespace {

aerosim::ScopeRef scope()
{
    aerosim::ScopeRef value;
    value.projectId = QStringLiteral("project");
    value.caseId = QStringLiteral("case");
    value.objectId = QStringLiteral("state");
    return value;
}

aerosim::workflow::ValidationInputStamp stamp(
    quint64 revision, const QString &version = QStringLiteral("v1"))
{
    aerosim::workflow::ValidationInputStamp value;
    value.scope = scope();
    value.definitionVersion = version;
    value.parameter.snapshotId = QStringLiteral("parameters");
    value.parameter.scope = value.scope;
    value.parameter.revision = revision;
    aerosim::workflow::ResourceInputStamp resource;
    resource.projectId = value.scope.projectId;
    resource.resourceId = QStringLiteral("mesh");
    resource.revision = revision;
    resource.hasVerifiedFingerprint = true;
    resource.fingerprint.status = aerosim::FingerprintStatus::Verified;
    resource.fingerprint.algorithm = QStringLiteral("sha256");
    resource.fingerprint.digest = QStringLiteral("digest-%1").arg(revision);
    resource.fingerprint.sizeBytes = 1024;
    value.resources.append(resource);
    return value;
}

aerosim::workflow::WorkflowDefinition definition(
    const QString &version = QStringLiteral("v1"))
{
    aerosim::workflow::WorkflowDefinition value;
    value.definitionVersion = version;
    aerosim::workflow::WorkflowStepDefinition step;
    step.stepId = QStringLiteral("step");
    step.applicabilityRuleId = QStringLiteral("always");
    value.steps.append(step);
    return value;
}

aerosim::workflow::ValidationCompletion completion(
    const aerosim::workflow::PendingValidation &pending,
    const aerosim::workflow::ValidationInputStamp &input)
{
    aerosim::workflow::ValidationCompletion value;
    value.requestId = pending.requestId;
    value.scope = pending.scope;
    value.stepId = pending.stepId;
    value.generation = pending.generation;
    value.inputStamp = input;
    value.outcome = aerosim::workflow::ValidationOutcome::Passed;
    value.completedAt = QDateTime::currentDateTimeUtc();
    return value;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application);
    aerosim_workflow_test::TestContext context;
    aerosim::workflow::registerWorkflowMetaTypes();
    AEROSIM_WORKFLOW_CHECK(context,
        QMetaType::type("aerosim::workflow::ValidationCompletion")
            != QMetaType::UnknownType);

    // One hundred deterministic A/B reorderings prove that a replaced
    // generation can never become current again.
    for (int index = 0; index < 100; ++index) {
        aerosim::workflow::ValidationCoordinator coordinator;
        const auto first = coordinator.begin(
            QStringLiteral("first"), scope(), QStringLiteral("step"),
            stamp(1));
        const auto second = coordinator.begin(
            QStringLiteral("second"), scope(), QStringLiteral("step"),
            stamp(1));
        AEROSIM_WORKFLOW_CHECK(context,
            first.ok() && second.ok()
                && second.value().generation > first.value().generation);
        AEROSIM_WORKFLOW_CHECK(context,
            !coordinator.accept(completion(first.value(), stamp(1)),
                                stamp(1)));
        AEROSIM_WORKFLOW_CHECK(context,
            coordinator.accept(completion(second.value(), stamp(1)),
                               stamp(1)));
        AEROSIM_WORKFLOW_CHECK(context,
            !coordinator.accept(completion(second.value(), stamp(1)),
                                stamp(1)));
    }

    // Exact-stamp mismatches are rejected independently of generation.
    QVector<aerosim::workflow::ValidationInputStamp> changedInputs;
    auto changed = stamp(1);
    changed.definitionVersion = QStringLiteral("v2");
    changedInputs.append(changed);
    changed = stamp(1);
    changed.parameter.snapshotId = QStringLiteral("other-parameters");
    changedInputs.append(changed);
    changed = stamp(1);
    changed.parameter.revision += 1;
    changed.resources[0].revision += 1;
    changed.resources[0].fingerprint.digest = QStringLiteral("digest-2");
    changedInputs.append(changed);
    changed = stamp(1);
    changed.resources[0].resourceId = QStringLiteral("other-mesh");
    changedInputs.append(changed);
    changed = stamp(1);
    changed.resources[0].fingerprint.digest = QStringLiteral("other-digest");
    changedInputs.append(changed);
    for (int index = 0; index < changedInputs.size(); ++index) {
        aerosim::workflow::ValidationCoordinator coordinator;
        const auto pending = coordinator.begin(
            QStringLiteral("stamp-%1").arg(index), scope(),
            QStringLiteral("step"), stamp(1));
        AEROSIM_WORKFLOW_CHECK(context, pending.ok());
        AEROSIM_WORKFLOW_CHECK(context,
            !coordinator.accept(completion(pending.value(), stamp(1)),
                                changedInputs.at(index)));
    }
    {
        aerosim::workflow::ValidationCoordinator coordinator;
        const auto pending = coordinator.begin(
            QStringLiteral("scope"), scope(), QStringLiteral("step"),
            stamp(1));
        auto wrongScope = completion(pending.value(), stamp(1));
        wrongScope.scope.caseId = QStringLiteral("other-case");
        AEROSIM_WORKFLOW_CHECK(context,
            !coordinator.accept(wrongScope, stamp(1)));
        coordinator.cancel(scope(), QStringLiteral("step"));
        AEROSIM_WORKFLOW_CHECK(context,
            !coordinator.accept(completion(pending.value(), stamp(1)),
                                stamp(1)));
    }
    {
        aerosim::workflow::ValidationCoordinator coordinator;
        const auto pending = coordinator.begin(
            QStringLiteral("destroy"), scope(), QStringLiteral("step"),
            stamp(1));
        coordinator.destroyScope(scope());
        AEROSIM_WORKFLOW_CHECK(context,
            !coordinator.accept(completion(pending.value(), stamp(1)),
                                stamp(1)));
    }

    aerosim::workflow::WorkflowService service;
    AEROSIM_WORKFLOW_CHECK(context,
        service.setDefinition(definition()).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        service.setCurrentInput(scope(), QStringLiteral("step"),
                                stamp(1)).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        service.saveEditing(scope(), QStringLiteral("step")).ok());
    int changes = 0;
    QObject::connect(&service,
        &aerosim::IWorkflowService::workflowViewV2Changed,
        [&changes](const aerosim::WorkflowViewV2 &) { ++changes; });
    const auto pending = service.beginValidation(
        scope(), QStringLiteral("step"), QStringLiteral("request"));
    AEROSIM_WORKFLOW_CHECK(context, pending.ok());
    const quint64 before = service.facts(scope()).value().factRevision;
    AEROSIM_WORKFLOW_CHECK(context,
        service.setCurrentInput(scope(), QStringLiteral("step"),
                                stamp(2)).ok());
    changes = 0;
    const auto late = completion(pending.value(), stamp(1));
    QMetaObject::invokeMethod(&service, "onValidationCompleted",
        Qt::QueuedConnection,
        Q_ARG(aerosim::workflow::ValidationCompletion, late));
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    AEROSIM_WORKFLOW_CHECK(context,
        service.facts(scope()).value().factRevision == before);
    AEROSIM_WORKFLOW_CHECK(context, changes == 0);

    const auto current = service.beginValidation(
        scope(), QStringLiteral("step"), QStringLiteral("current"));
    auto currentResult = completion(current.value(), stamp(2));
    currentResult.outcome = aerosim::workflow::ValidationOutcome::Failed;
    QMetaObject::invokeMethod(&service, "onValidationCompleted",
        Qt::QueuedConnection,
        Q_ARG(aerosim::workflow::ValidationCompletion, currentResult));
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    AEROSIM_WORKFLOW_CHECK(context,
        service.facts(scope()).value().factRevision > before);
    AEROSIM_WORKFLOW_CHECK(context, changes == 1);

    AEROSIM_WORKFLOW_CHECK(context,
        service.setDefinition(definition(QStringLiteral("v2"))).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        service.viewV2(scope()).value().steps.first().validation
            == aerosim::ValidationState::Stale);
    AEROSIM_WORKFLOW_CHECK(context,
        !service.setCurrentInput(scope(), QStringLiteral("step"),
                                 stamp(3, QStringLiteral("v1"))).ok());
    service.shutdown();
    AEROSIM_WORKFLOW_CHECK(context,
        !service.beginValidation(scope(), QStringLiteral("step"),
                                 QStringLiteral("after-shutdown")).ok());
    return context.result();
}
