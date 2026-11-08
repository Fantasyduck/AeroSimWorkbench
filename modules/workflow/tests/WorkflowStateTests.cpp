#include "WorkflowTestSupport.h"

#include "aerosim/workflow/WorkflowService.h"

#include <QCoreApplication>

namespace {

aerosim::ScopeRef makeScope()
{
    aerosim::ScopeRef scope;
    scope.projectId = QStringLiteral("project");
    scope.caseId = QStringLiteral("case");
    scope.objectId = QStringLiteral("state");
    return scope;
}

aerosim::workflow::ValidationInputStamp makeStamp(quint64 revision)
{
    aerosim::workflow::ValidationInputStamp stamp;
    stamp.scope = makeScope();
    stamp.definitionVersion = QStringLiteral("workflow-v2");
    stamp.parameter.snapshotId = QStringLiteral("parameters");
    stamp.parameter.scope = stamp.scope;
    stamp.parameter.revision = revision;
    return stamp;
}

aerosim::workflow::WorkflowDefinition makeDefinition()
{
    aerosim::workflow::WorkflowDefinition definition;
    definition.definitionVersion = QStringLiteral("workflow-v2");
    aerosim::workflow::WorkflowStepDefinition configured;
    configured.stepId = QStringLiteral("configured");
    configured.summary = QStringLiteral("Configured step");
    configured.targetPageId = QStringLiteral("parameters");
    configured.applicabilityRuleId = QStringLiteral("always");
    definition.steps.append(configured);
    aerosim::workflow::WorkflowStepDefinition neverValidated = configured;
    neverValidated.stepId = QStringLiteral("never-validated");
    definition.steps.append(neverValidated);
    return definition;
}

const aerosim::WorkflowStepViewV2 *findStep(
    const aerosim::WorkflowViewV2 &view, const QString &stepId)
{
    for (const aerosim::WorkflowStepViewV2 &step : view.steps) {
        if (step.stepId == stepId)
            return &step;
    }
    return nullptr;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application);
    aerosim_workflow_test::TestContext context;
    aerosim::workflow::WorkflowService service;
    const aerosim::ScopeRef scope = makeScope();
    AEROSIM_WORKFLOW_CHECK(context, service.setDefinition(makeDefinition()).ok());

    aerosim::WorkflowViewV2 view = service.viewV2(scope).value();
    const aerosim::WorkflowStepViewV2 *configured =
        findStep(view, QStringLiteral("configured"));
    AEROSIM_WORKFLOW_CHECK(context, configured != nullptr);
    AEROSIM_WORKFLOW_CHECK(context,
        configured->completion == aerosim::CompletionState::NotStarted);
    AEROSIM_WORKFLOW_CHECK(context,
        configured->validation == aerosim::ValidationState::NotValidated);
    AEROSIM_WORKFLOW_CHECK(context, configured->applicable);
    AEROSIM_WORKFLOW_CHECK(context,
        configured->applicabilityRuleId == QStringLiteral("always"));

    const aerosim::workflow::ValidationInputStamp revision1 = makeStamp(1);
    AEROSIM_WORKFLOW_CHECK(context,
        service.setCurrentInput(scope, QStringLiteral("configured"), revision1).ok());
    const quint64 saveRevision =
        service.saveEditing(scope, QStringLiteral("configured")).value();
    AEROSIM_WORKFLOW_CHECK(context, saveRevision == 1);
    AEROSIM_WORKFLOW_CHECK(context,
        service.acceptValidation(scope, QStringLiteral("configured"),
            aerosim::workflow::ValidationOutcome::Passed, revision1).value() == 2);

    view = service.viewV2(scope).value();
    configured = findStep(view, QStringLiteral("configured"));
    AEROSIM_WORKFLOW_CHECK(context,
        configured->completion == aerosim::CompletionState::Completed);
    AEROSIM_WORKFLOW_CHECK(context,
        configured->validation == aerosim::ValidationState::Valid);
    const quint64 committedFactRevision = configured->factRevision;
    const quint64 validViewRevision = view.viewRevision;

    AEROSIM_WORKFLOW_CHECK(context,
        service.beginEditing(scope, QStringLiteral("configured"),
            QStringLiteral("session-1")).ok());
    view = service.viewV2(scope).value();
    configured = findStep(view, QStringLiteral("configured"));
    AEROSIM_WORKFLOW_CHECK(context,
        configured->completion == aerosim::CompletionState::InProgress);
    AEROSIM_WORKFLOW_CHECK(context,
        configured->validation == aerosim::ValidationState::Valid);
    AEROSIM_WORKFLOW_CHECK(context,
        configured->factRevision == committedFactRevision);
    AEROSIM_WORKFLOW_CHECK(context, view.viewRevision > validViewRevision);

    const aerosim::workflow::WorkflowScopeFacts factsBeforeCancel =
        service.facts(scope).value();
    AEROSIM_WORKFLOW_CHECK(context,
        service.cancelEditing(scope, QStringLiteral("configured")).ok());
    const aerosim::workflow::WorkflowScopeFacts factsAfterCancel =
        service.facts(scope).value();
    AEROSIM_WORKFLOW_CHECK(context,
        factsBeforeCancel.factRevision == factsAfterCancel.factRevision);
    AEROSIM_WORKFLOW_CHECK(context,
        factsAfterCancel.steps.first().validationFact.inputStamp.parameter.revision == 1);

    AEROSIM_WORKFLOW_CHECK(context,
        service.beginEditing(scope, QStringLiteral("configured"),
            QStringLiteral("session-2")).ok());
    const aerosim::workflow::WorkflowScopeFacts factsBeforeFailure =
        service.facts(scope).value();
    AEROSIM_WORKFLOW_CHECK(context,
        service.failEditingSave(scope, QStringLiteral("configured")).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        service.facts(scope).value().factRevision
            == factsBeforeFailure.factRevision);

    const aerosim::workflow::ValidationInputStamp revision2 = makeStamp(2);
    AEROSIM_WORKFLOW_CHECK(context,
        service.beginEditing(scope, QStringLiteral("configured"),
            QStringLiteral("session-3")).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        service.setCurrentInput(scope, QStringLiteral("configured"), revision2).ok());
    const quint64 savedNewInput =
        service.saveEditing(scope, QStringLiteral("configured")).value();
    AEROSIM_WORKFLOW_CHECK(context, savedNewInput == 3);
    view = service.viewV2(scope).value();
    configured = findStep(view, QStringLiteral("configured"));
    AEROSIM_WORKFLOW_CHECK(context,
        configured->completion == aerosim::CompletionState::Completed);
    AEROSIM_WORKFLOW_CHECK(context,
        configured->validation == aerosim::ValidationState::Stale);
    AEROSIM_WORKFLOW_CHECK(context,
        service.facts(scope).value().steps.first().validationFact.inputStamp.parameter.revision == 1);

    AEROSIM_WORKFLOW_CHECK(context,
        service.setCurrentInput(scope, QStringLiteral("never-validated"), revision2).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        service.saveEditing(scope, QStringLiteral("never-validated")).ok());
    view = service.viewV2(scope).value();
    const aerosim::WorkflowStepViewV2 *neverValidated =
        findStep(view, QStringLiteral("never-validated"));
    AEROSIM_WORKFLOW_CHECK(context,
        neverValidated->completion == aerosim::CompletionState::Completed);
    AEROSIM_WORKFLOW_CHECK(context,
        neverValidated->validation == aerosim::ValidationState::NotValidated);

    const quint64 unchangedViewRevision = view.viewRevision;
    AEROSIM_WORKFLOW_CHECK(context,
        service.setCurrentInput(scope, QStringLiteral("never-validated"), revision2).ok());
    AEROSIM_WORKFLOW_CHECK(context,
        service.viewV2(scope).value().viewRevision == unchangedViewRevision);

    return context.result();
}
