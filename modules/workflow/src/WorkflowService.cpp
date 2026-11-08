#include "aerosim/workflow/WorkflowService.h"

#include "aerosim/workflow/ValidationStampMatcher.h"
#include "aerosim/workflow/ValidationCoordinator.h"

#include <QThread>
#include <QUuid>

#include <algorithm>

namespace aerosim {
namespace workflow {
namespace {

bool scopeEquals(const ScopeRef &left, const ScopeRef &right)
{
    return left.projectId == right.projectId
        && left.caseId == right.caseId
        && left.objectId == right.objectId;
}

bool scopeValid(const ScopeRef &scope)
{
    return !scope.projectId.isEmpty();
}

ValidationIssue serviceIssue(const QString &code, const QString &message,
                             const QString &targetPageId = QString())
{
    ValidationIssue issue;
    issue.code = code;
    issue.message = message;
    issue.targetPageId = targetPageId;
    return issue;
}

template <typename T>
AccessResult<T> serviceFailure(AccessStatus status, const QString &code,
                               const QString &message)
{
    return AccessResult<T>::failure(status, code, message);
}

bool issueEquals(const ValidationIssue &left, const ValidationIssue &right)
{
    return left.severity == right.severity && left.code == right.code
        && left.message == right.message && left.scopeId == right.scopeId
        && left.objectId == right.objectId && left.fieldKey == right.fieldKey
        && left.targetPageId == right.targetPageId;
}

bool blockerEquals(const DependencyBlockerView &left,
                   const DependencyBlockerView &right)
{
    return left.dependencyStepId == right.dependencyStepId
        && left.code == right.code && left.message == right.message
        && left.targetPageId == right.targetPageId;
}

bool actionEquals(const WorkflowActionView &left,
                  const WorkflowActionView &right)
{
    return left.actionId == right.actionId && left.enabled == right.enabled
        && left.reason == right.reason
        && left.targetPageId == right.targetPageId
        && left.scopeId == right.scopeId && left.objectId == right.objectId
        && left.fieldKey == right.fieldKey;
}

bool stepEquals(const WorkflowStepViewV2 &left,
                const WorkflowStepViewV2 &right)
{
    if (left.stepId != right.stepId || !scopeEquals(left.scope, right.scope)
        || left.applicable != right.applicable
        || left.applicabilityRuleId != right.applicabilityRuleId
        || left.completion != right.completion
        || left.validation != right.validation
        || left.summary != right.summary
        || left.targetPageId != right.targetPageId
        || left.factRevision != right.factRevision
        || left.blockers.size() != right.blockers.size()) {
        return false;
    }
    for (int index = 0; index < left.blockers.size(); ++index) {
        if (!blockerEquals(left.blockers.at(index), right.blockers.at(index)))
            return false;
    }
    return true;
}

bool viewContentEquals(const WorkflowViewV2 &left,
                       const WorkflowViewV2 &right)
{
    if (!scopeEquals(left.scope, right.scope)
        || left.definitionVersion != right.definitionVersion
        || left.nextSuggestion != right.nextSuggestion
        || left.steps.size() != right.steps.size()
        || left.actions.size() != right.actions.size()
        || left.issues.size() != right.issues.size()) {
        return false;
    }
    for (int index = 0; index < left.steps.size(); ++index) {
        if (!stepEquals(left.steps.at(index), right.steps.at(index)))
            return false;
    }
    for (int index = 0; index < left.actions.size(); ++index) {
        if (!actionEquals(left.actions.at(index), right.actions.at(index)))
            return false;
    }
    for (int index = 0; index < left.issues.size(); ++index) {
        if (!issueEquals(left.issues.at(index), right.issues.at(index)))
            return false;
    }
    return true;
}

} // namespace

class WorkflowService::Impl {
public:
    struct ScopeState {
        WorkflowScopeFacts facts;
        QVector<WorkflowStepCurrentInput> currentInputs;
        QVector<EditingDraftOverlay> drafts;
        quint64 viewRevision = 0;
        bool hasCachedView = false;
        WorkflowViewV2 cachedView;
    };

    WorkflowDefinition definition;
    QVector<ScopeState> scopes;
    ValidationCoordinator validationCoordinator;

    const WorkflowStepDefinition *stepDefinition(const QString &stepId) const
    {
        for (const WorkflowStepDefinition &step : definition.steps) {
            if (step.stepId == stepId)
                return &step;
        }
        return nullptr;
    }

    ScopeState *findScope(const ScopeRef &scope)
    {
        for (ScopeState &state : scopes) {
            if (scopeEquals(state.facts.scope, scope))
                return &state;
        }
        return nullptr;
    }

    const ScopeState *findScope(const ScopeRef &scope) const
    {
        for (const ScopeState &state : scopes) {
            if (scopeEquals(state.facts.scope, scope))
                return &state;
        }
        return nullptr;
    }

    ScopeState &ensureScope(const ScopeRef &scope)
    {
        ScopeState *existing = findScope(scope);
        if (existing)
            return *existing;
        ScopeState state;
        state.facts.scope = scope;
        state.facts.definitionVersion = definition.definitionVersion;
        scopes.append(state);
        return scopes.last();
    }

    WorkflowStepFacts &ensureStepFacts(ScopeState &state,
                                       const QString &stepId)
    {
        for (WorkflowStepFacts &facts : state.facts.steps) {
            if (facts.stepId == stepId)
                return facts;
        }
        WorkflowStepFacts facts;
        facts.stepId = stepId;
        state.facts.steps.append(facts);
        return state.facts.steps.last();
    }

    const WorkflowStepCurrentInput *findCurrentInput(
        const ScopeState &state, const QString &stepId) const
    {
        for (const WorkflowStepCurrentInput &input : state.currentInputs) {
            if (input.stepId == stepId)
                return &input;
        }
        return nullptr;
    }

    WorkflowDerivationContext derivationContext(
        const ScopeState &state, quint64 revision) const
    {
        WorkflowDerivationContext context;
        context.scope = state.facts.scope;
        context.definition = definition;
        context.facts = state.facts;
        context.currentInputs = state.currentInputs;
        context.drafts = state.drafts;
        context.viewRevision = revision;
        return context;
    }

    WorkflowViewV2 derive(const ScopeState &state, quint64 revision) const
    {
        return deriveWorkflowView(derivationContext(state, revision));
    }

    bool publishIfChanged(WorkflowService *service, ScopeState &state) const
    {
        WorkflowViewV2 candidate = derive(state, state.viewRevision);
        if (state.hasCachedView
            && viewContentEquals(candidate, state.cachedView)) {
            return false;
        }
        ++state.viewRevision;
        state.cachedView = derive(state, state.viewRevision);
        state.hasCachedView = true;
        emit service->workflowViewV2Changed(state.cachedView);
        return true;
    }

    quint64 nextFactRevision(const ScopeState &state) const
    {
        quint64 maximum = state.facts.factRevision;
        for (const WorkflowStepFacts &facts : state.facts.steps)
            maximum = std::max(maximum, facts.factRevision);
        return maximum + 1;
    }
};

WorkflowService::WorkflowService(QObject *parent)
    : IWorkflowService(parent), impl_(new Impl)
{
    registerWorkflowMetaTypes();
}

WorkflowService::~WorkflowService()
{
    shutdown();
}

AccessResult<WorkflowViewV2> WorkflowService::viewV2(
    const ScopeRef &scope) const
{
    if (!scopeValid(scope)) {
        return serviceFailure<WorkflowViewV2>(
            AccessStatus::InvalidArgument,
            QStringLiteral("workflow.scope.invalid"),
            QStringLiteral("workflow scope must identify a project"));
    }
    if (impl_->definition.definitionVersion.isEmpty()) {
        return serviceFailure<WorkflowViewV2>(
            AccessStatus::Unavailable,
            QStringLiteral("workflow.definition.unavailable"),
            QStringLiteral("workflow definition is not configured"));
    }
    const Impl::ScopeState *state = impl_->findScope(scope);
    if (!state) {
        Impl::ScopeState empty;
        empty.facts.scope = scope;
        empty.facts.definitionVersion = impl_->definition.definitionVersion;
        return AccessResult<WorkflowViewV2>::success(impl_->derive(empty, 0));
    }
    return AccessResult<WorkflowViewV2>::success(
        state->hasCachedView ? state->cachedView
                             : impl_->derive(*state, state->viewRevision));
}

ValidationResult WorkflowService::requestRun(const RunRequest &request)
{
    ValidationResult result;
    const Impl::ScopeState *state = impl_->findScope(request.scope);
    if (!state) {
        result.issues.append(serviceIssue(
            QStringLiteral("workflow.scope.not_found"),
            QStringLiteral("workflow scope was not found")));
        return result;
    }

    ActionAuthorization authorization = authorizeWorkflowAction(
        impl_->derivationContext(*state, state->viewRevision),
        QStringLiteral("run"));
    if (!authorization.allowed()) {
        result.issues = authorization.issues;
        return result;
    }

    const WorkflowActionRule *runRule = nullptr;
    for (const WorkflowActionRule &rule : impl_->definition.actions) {
        if (rule.actionId == QStringLiteral("run")) {
            runRule = &rule;
            break;
        }
    }
    if (!runRule) {
        result.issues = authorization.issues;
        return result;
    }
    for (const QString &stepId : runRule->requiredStepIds) {
        const WorkflowStepCurrentInput *input =
            impl_->findCurrentInput(*state, stepId);
        if (!input
            || input->inputStamp.parameter.snapshotId
                != request.inputSnapshot.snapshotId
            || !scopeEquals(input->inputStamp.parameter.scope,
                            request.inputSnapshot.scope)
            || input->inputStamp.parameter.revision
                != request.inputSnapshot.revision) {
            result.issues.append(serviceIssue(
                QStringLiteral("workflow.action.input_changed"),
                QStringLiteral("requested input is not the currently authorized input"),
                authorization.action.targetPageId));
            return result;
        }
    }

    TaskCommand command;
    command.commandId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    command.taskId = command.commandId;
    command.caseId = request.scope.caseId;
    command.type = TaskCommandType::Start;
    command.inputSnapshotId = request.inputSnapshot.snapshotId;
    command.batchPlanId = request.batchPlanId;
    command.batchPlanVersion = request.batchPlanVersion;
    emit taskCommandRequested(command);
    result.allowed = true;
    return result;
}

AccessResult<WorkflowDefinition> WorkflowService::setDefinition(
    const WorkflowDefinition &definition)
{
    const AccessResult<WorkflowDefinition> validated =
        validateWorkflowDefinition(definition);
    if (!validated.ok())
        return validated;
    impl_->definition = validated.value();
    for (Impl::ScopeState &state : impl_->scopes)
        impl_->publishIfChanged(this, state);
    return validated;
}

AccessResult<quint64> WorkflowService::setCurrentInput(
    const ScopeRef &scope, const QString &stepId,
    const ValidationInputStamp &inputStamp)
{
    if (!scopeValid(scope) || !scopeEquals(scope, inputStamp.scope)
        || !impl_->stepDefinition(stepId)) {
        return serviceFailure<quint64>(
            AccessStatus::InvalidArgument,
            QStringLiteral("workflow.input.invalid"),
            QStringLiteral("current input must identify a configured step and scope"));
    }
    const AccessResult<ValidationInputStamp> normalized =
        normalizeValidationInputStamp(inputStamp);
    if (!normalized.ok()) {
        return serviceFailure<quint64>(normalized.status(),
                                       normalized.errorCode(),
                                       normalized.message());
    }
    if (normalized.value().definitionVersion
        != impl_->definition.definitionVersion) {
        return serviceFailure<quint64>(AccessStatus::Conflict,
            QStringLiteral("workflow.validation.definition_changed"),
            QStringLiteral("input stamp does not match the current workflow definition"));
    }
    Impl::ScopeState &state = impl_->ensureScope(scope);
    bool replaced = false;
    for (WorkflowStepCurrentInput &input : state.currentInputs) {
        if (input.stepId == stepId) {
            input.inputStamp = normalized.value();
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        WorkflowStepCurrentInput input;
        input.stepId = stepId;
        input.inputStamp = normalized.value();
        state.currentInputs.append(input);
    }
    impl_->publishIfChanged(this, state);
    return AccessResult<quint64>::success(state.facts.factRevision);
}

AccessResult<quint64> WorkflowService::beginEditing(
    const ScopeRef &scope, const QString &stepId, const QString &sessionId)
{
    if (!scopeValid(scope) || !impl_->stepDefinition(stepId)
        || sessionId.isEmpty()) {
        return serviceFailure<quint64>(
            AccessStatus::InvalidArgument,
            QStringLiteral("workflow.edit.invalid"),
            QStringLiteral("editing requires a configured step and session"));
    }
    Impl::ScopeState &state = impl_->ensureScope(scope);
    for (EditingDraftOverlay &draft : state.drafts) {
        if (draft.stepId == stepId) {
            draft.sessionId = sessionId;
            impl_->publishIfChanged(this, state);
            return AccessResult<quint64>::success(state.facts.factRevision);
        }
    }
    EditingDraftOverlay draft;
    draft.scope = scope;
    draft.stepId = stepId;
    draft.baseFactRevision = state.facts.factRevision;
    draft.sessionId = sessionId;
    state.drafts.append(draft);
    impl_->publishIfChanged(this, state);
    return AccessResult<quint64>::success(state.facts.factRevision);
}

AccessResult<quint64> WorkflowService::cancelEditing(
    const ScopeRef &scope, const QString &stepId)
{
    Impl::ScopeState *state = impl_->findScope(scope);
    if (!state) {
        return serviceFailure<quint64>(
            AccessStatus::NotFound, QStringLiteral("workflow.scope.not_found"),
            QStringLiteral("workflow scope was not found"));
    }
    for (int index = 0; index < state->drafts.size(); ++index) {
        if (state->drafts.at(index).stepId == stepId) {
            state->drafts.removeAt(index);
            impl_->publishIfChanged(this, *state);
            break;
        }
    }
    return AccessResult<quint64>::success(state->facts.factRevision);
}

AccessResult<quint64> WorkflowService::failEditingSave(
    const ScopeRef &scope, const QString &stepId)
{
    return cancelEditing(scope, stepId);
}

AccessResult<quint64> WorkflowService::saveEditing(
    const ScopeRef &scope, const QString &stepId)
{
    Impl::ScopeState *state = impl_->findScope(scope);
    if (!state || !impl_->stepDefinition(stepId)) {
        return serviceFailure<quint64>(
            AccessStatus::NotFound, QStringLiteral("workflow.step.not_found"),
            QStringLiteral("workflow step was not found"));
    }
    const quint64 revision = impl_->nextFactRevision(*state);
    WorkflowStepFacts &facts = impl_->ensureStepFacts(*state, stepId);
    facts.committedCompletion = CompletionState::Completed;
    facts.factRevision = revision;
    facts.updatedAt = QDateTime::currentDateTimeUtc();
    state->facts.factRevision = revision;
    state->facts.definitionVersion = impl_->definition.definitionVersion;
    for (int index = state->drafts.size() - 1; index >= 0; --index) {
        if (state->drafts.at(index).stepId == stepId)
            state->drafts.removeAt(index);
    }
    impl_->publishIfChanged(this, *state);
    return AccessResult<quint64>::success(revision);
}

AccessResult<quint64> WorkflowService::acceptValidation(
    const ScopeRef &scope, const QString &stepId, ValidationOutcome outcome,
    const ValidationInputStamp &inputStamp,
    const QVector<ValidationIssue> &issues, const QDateTime &validatedAt)
{
    Impl::ScopeState *state = impl_->findScope(scope);
    if (!state || !impl_->stepDefinition(stepId)) {
        return serviceFailure<quint64>(
            AccessStatus::NotFound, QStringLiteral("workflow.step.not_found"),
            QStringLiteral("workflow step was not found"));
    }
    const Impl::ScopeState &constState = *state;
    const WorkflowStepCurrentInput *current =
        impl_->findCurrentInput(constState, stepId);
    if (!current) {
        return serviceFailure<quint64>(
            AccessStatus::Conflict,
            QStringLiteral("workflow.validation.input_changed"),
            QStringLiteral("validation input is no longer current"));
    }
    if (inputStamp.definitionVersion != impl_->definition.definitionVersion) {
        return serviceFailure<quint64>(AccessStatus::Conflict,
            QStringLiteral("workflow.validation.definition_changed"),
            QStringLiteral("validation was produced for an old workflow definition"));
    }
    const AccessResult<bool> match = validationInputStampsMatch(
        inputStamp, current->inputStamp);
    if (!match.ok() || !match.value()) {
        return serviceFailure<quint64>(
            AccessStatus::Conflict,
            QStringLiteral("workflow.validation.input_changed"),
            QStringLiteral("validation input is no longer current"));
    }
    const quint64 revision = impl_->nextFactRevision(*state);
    WorkflowStepFacts &facts = impl_->ensureStepFacts(*state, stepId);
    facts.hasValidationFact = true;
    facts.validationFact.outcome = outcome;
    facts.validationFact.issues = issues;
    facts.validationFact.inputStamp =
        normalizeValidationInputStamp(inputStamp).value();
    facts.validationFact.validatedAt = validatedAt;
    facts.factRevision = revision;
    facts.updatedAt = validatedAt;
    state->facts.factRevision = revision;
    state->facts.definitionVersion = impl_->definition.definitionVersion;
    impl_->publishIfChanged(this, *state);
    return AccessResult<quint64>::success(revision);
}

AccessResult<WorkflowScopeFacts> WorkflowService::facts(
    const ScopeRef &scope) const
{
    const Impl::ScopeState *state = impl_->findScope(scope);
    if (!state) {
        return serviceFailure<WorkflowScopeFacts>(
            AccessStatus::NotFound, QStringLiteral("workflow.scope.not_found"),
            QStringLiteral("workflow scope was not found"));
    }
    return AccessResult<WorkflowScopeFacts>::success(state->facts);
}

void WorkflowService::onTaskEvent(const TaskEvent &event)
{
    Q_UNUSED(event);
}

AccessResult<PendingValidation> WorkflowService::beginValidation(
    const ScopeRef &scope, const QString &stepId, const QString &requestId)
{
    Impl::ScopeState *state = impl_->findScope(scope);
    if (!state || !impl_->stepDefinition(stepId)) {
        return serviceFailure<PendingValidation>(AccessStatus::NotFound,
            QStringLiteral("workflow.step.not_found"),
            QStringLiteral("workflow step was not found"));
    }
    const WorkflowStepCurrentInput *input = impl_->findCurrentInput(*state, stepId);
    if (!input) {
        return serviceFailure<PendingValidation>(AccessStatus::Unavailable,
            QStringLiteral("workflow.validation.input_unavailable"),
            QStringLiteral("current validation input is unavailable"));
    }
    return impl_->validationCoordinator.begin(requestId, scope, stepId,
                                               input->inputStamp);
}

void WorkflowService::cancelValidation(const ScopeRef &scope,
                                       const QString &stepId)
{
    impl_->validationCoordinator.cancel(scope, stepId);
}

void WorkflowService::destroyScope(const ScopeRef &scope)
{
    impl_->validationCoordinator.destroyScope(scope);
    for (int i = impl_->scopes.size() - 1; i >= 0; --i) {
        if (scopeEquals(impl_->scopes.at(i).facts.scope, scope))
            impl_->scopes.removeAt(i);
    }
}

void WorkflowService::shutdown()
{
    if (impl_)
        impl_->validationCoordinator.shutdown();
}

AccessResult<WorkflowFactsSnapshot> WorkflowService::snapshot(
    const ScopeRef &scope) const
{
    const Impl::ScopeState *state = impl_->findScope(scope);
    if (!state) {
        return serviceFailure<WorkflowFactsSnapshot>(
            AccessStatus::NotFound,
            QStringLiteral("workflow.snapshot.scope_not_found"),
            QStringLiteral("workflow scope facts were not found"));
    }
    return makeWorkflowFactsSnapshot(state->facts);
}

AccessResult<quint64> WorkflowService::restoreSnapshot(
    const WorkflowFactsSnapshot &snapshotValue)
{
    const AccessResult<WorkflowFactsSnapshot> validated =
        validateWorkflowFactsSnapshot(snapshotValue);
    if (!validated.ok()) {
        return serviceFailure<quint64>(validated.status(),
                                       validated.errorCode(),
                                       validated.message());
    }

    const WorkflowFactsSnapshot &incoming = validated.value();
    Impl::ScopeState *existing = impl_->findScope(incoming.scope);
    const bool hasStoredFacts = existing
        && (existing->facts.factRevision != 0
            || !existing->facts.steps.isEmpty());

    if (hasStoredFacts) {
        const AccessResult<WorkflowFactsSnapshot> current =
            makeWorkflowFactsSnapshot(existing->facts, incoming.capturedAt);
        if (!current.ok()) {
            return serviceFailure<quint64>(
                AccessStatus::InternalError,
                QStringLiteral("workflow.snapshot.current_facts_invalid"),
                QStringLiteral("stored workflow facts cannot be snapshotted"));
        }
        if (incoming.factRevision < current.value().factRevision
            || (incoming.factRevision == current.value().factRevision
                && !workflowSnapshotFactsEqual(incoming, current.value()))) {
            return serviceFailure<quint64>(
                AccessStatus::Conflict,
                QStringLiteral("workflow.snapshot.revision_conflict"),
                QStringLiteral("snapshot would replace facts at the same or a newer revision"));
        }
        if (incoming.factRevision == current.value().factRevision)
            return AccessResult<quint64>::success(incoming.factRevision);
    }

    // Finish validation before the first mutation. Runtime-only drafts and
    // pending validation requests are intentionally excluded from snapshots.
    Impl::ScopeState replacement;
    if (existing) {
        replacement.currentInputs = existing->currentInputs;
        replacement.viewRevision = existing->viewRevision;
        replacement.hasCachedView = existing->hasCachedView;
        replacement.cachedView = existing->cachedView;
    }
    replacement.facts = workflowFactsFromSnapshot(incoming);

    impl_->validationCoordinator.destroyScope(incoming.scope);
    if (existing) {
        *existing = replacement;
        impl_->publishIfChanged(this, *existing);
    } else {
        impl_->scopes.append(replacement);
        impl_->publishIfChanged(this, impl_->scopes.last());
    }
    return AccessResult<quint64>::success(incoming.factRevision);
}

void WorkflowService::onValidationCompleted(
    aerosim::workflow::ValidationCompletion completion)
{
    Q_ASSERT(QThread::currentThread() == thread());
    Impl::ScopeState *state = impl_->findScope(completion.scope);
    if (!state)
        return;
    const WorkflowStepCurrentInput *input =
        impl_->findCurrentInput(*state, completion.stepId);
    if (!input || !impl_->validationCoordinator.accept(
                      completion, input->inputStamp))
        return;
    acceptValidation(completion.scope, completion.stepId, completion.outcome,
                     completion.inputStamp, completion.issues,
                     completion.completedAt);
}

} // namespace workflow
} // namespace aerosim
