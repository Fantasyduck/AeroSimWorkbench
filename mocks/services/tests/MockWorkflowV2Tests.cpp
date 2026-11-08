#include "MockWorkflowService.h"

#include <QCoreApplication>

#include <iostream>

namespace {

#define CHECK(condition) \
    do { if (!(condition)) { \
        std::cerr << "Mock Workflow V2 check failed at line " << __LINE__ \
                  << '\n'; return 1; \
    } } while (false)

aerosim::ScopeRef scope()
{
    aerosim::ScopeRef value;
    value.projectId = QStringLiteral("project");
    value.caseId = QStringLiteral("case");
    value.objectId = QStringLiteral("state");
    return value;
}

aerosim::workflow::ValidationInputStamp stamp(quint64 revision)
{
    aerosim::workflow::ValidationInputStamp value;
    value.scope = scope();
    value.definitionVersion = QStringLiteral("mock-controlled-v2");
    value.parameter.snapshotId = QStringLiteral("parameters");
    value.parameter.scope = value.scope;
    value.parameter.revision = revision;
    return value;
}

aerosim::workflow::WorkflowDefinition definition()
{
    aerosim::workflow::WorkflowDefinition value;
    value.definitionVersion = QStringLiteral("mock-controlled-v2");
    aerosim::workflow::WorkflowStepDefinition upstream;
    upstream.stepId = QStringLiteral("upstream");
    upstream.applicabilityRuleId = QStringLiteral("mock.always");
    value.steps.append(upstream);
    aerosim::workflow::WorkflowStepDefinition downstream;
    downstream.stepId = QStringLiteral("downstream");
    downstream.applicabilityRuleId = QStringLiteral("mock.always");
    aerosim::workflow::DependencyRequirement dependency;
    dependency.requirementId = QStringLiteral("upstream-current-validation");
    dependency.dependencyStepId = upstream.stepId;
    dependency.conditionKind = aerosim::workflow::
        DependencyConditionKind::CurrentValidationValid;
    dependency.code = QStringLiteral("mock.upstream.invalid");
    dependency.message = QStringLiteral("Mock upstream is not valid");
    downstream.dependencies.append(dependency);
    value.steps.append(downstream);
    aerosim::workflow::WorkflowActionRule run;
    run.actionId = QStringLiteral("run");
    run.requiresCurrentValidation = true;
    run.requiredStepIds.append(downstream.stepId);
    value.actions.append(run);
    return value;
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

    aerosim::MockWorkflowService mock;
    aerosim::workflow::WorkflowService &engine = mock.controlledService();
    CHECK(engine.setDefinition(definition()).ok());
    for (const QString &stepId : {QStringLiteral("upstream"),
                                  QStringLiteral("downstream")}) {
        CHECK(engine.setCurrentInput(scope(), stepId, stamp(1)).ok());
        CHECK(engine.saveEditing(scope(), stepId).ok());
        CHECK(engine.acceptValidation(scope(), stepId,
            aerosim::workflow::ValidationOutcome::Passed, stamp(1)).ok());
    }

    int v2Signals = 0;
    QObject::connect(&mock, &aerosim::IWorkflowService::workflowViewV2Changed,
        [&v2Signals](const aerosim::WorkflowViewV2 &) {
            ++v2Signals;
        });

    CHECK(engine.setCurrentInput(scope(), QStringLiteral("upstream"),
                                 stamp(2)).ok());
    CHECK(v2Signals == 1);
    aerosim::WorkflowViewV2 view = mock.viewV2(scope()).value();
    CHECK(findStep(view, QStringLiteral("upstream"))->validation
          == aerosim::ValidationState::Stale);
    CHECK(findStep(view, QStringLiteral("downstream"))->blockers.size() == 1);

    CHECK(engine.acceptValidation(scope(), QStringLiteral("upstream"),
        aerosim::workflow::ValidationOutcome::Passed, stamp(2)).ok());
    view = mock.viewV2(scope()).value();
    CHECK(findStep(view, QStringLiteral("downstream"))->blockers.isEmpty());

    const auto first = engine.beginValidation(
        scope(), QStringLiteral("downstream"), QStringLiteral("first"));
    const auto second = engine.beginValidation(
        scope(), QStringLiteral("downstream"), QStringLiteral("second"));
    CHECK(first.ok() && second.ok());
    aerosim::workflow::ValidationCompletion completion;
    completion.scope = scope();
    completion.stepId = QStringLiteral("downstream");
    completion.inputStamp = stamp(1);
    completion.outcome = aerosim::workflow::ValidationOutcome::Failed;
    completion.completedAt = QDateTime::currentDateTimeUtc();
    completion.requestId = QStringLiteral("first");
    completion.generation = first.value().generation;
    const quint64 beforeLate = engine.facts(scope()).value().factRevision;
    engine.onValidationCompleted(completion);
    CHECK(engine.facts(scope()).value().factRevision == beforeLate);
    completion.requestId = QStringLiteral("second");
    completion.generation = second.value().generation;
    engine.onValidationCompleted(completion);
    CHECK(engine.facts(scope()).value().factRevision > beforeLate);

    aerosim::RunRequest request;
    request.scope = scope();
    request.inputSnapshot = stamp(1).parameter;
    int starts = 0;
    QObject::connect(&mock, &aerosim::IWorkflowService::taskCommandRequested,
        [&starts](const aerosim::TaskCommand &command) {
            if (command.type == aerosim::TaskCommandType::Start)
                ++starts;
        });
    CHECK(!mock.requestRun(request).allowed);
    CHECK(starts == 0);
    CHECK(engine.acceptValidation(scope(), QStringLiteral("downstream"),
        aerosim::workflow::ValidationOutcome::Passed, stamp(1)).ok());
    CHECK(mock.requestRun(request).allowed);
    CHECK(starts == 1);

    int observed = 0;
    QObject::connect(&mock, &aerosim::MockWorkflowService::mockEventObserved,
        [&observed](const aerosim::TaskEvent &) { ++observed; });
    aerosim::TaskEvent event;
    event.taskId = QStringLiteral("legacy-task");
    mock.onTaskEvent(event);
    CHECK(observed == 1);

    const aerosim::WorkflowViewV2 v2 = mock.viewV2(scope()).value();
    CHECK(v2.viewRevision > 0);
    return 0;
}
