#include "aerosim/workflow/WorkflowSnapshot.h"

#include "aerosim/workflow/ValidationStampMatcher.h"

#include <QSet>

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

bool issueEquals(const ValidationIssue &left, const ValidationIssue &right)
{
    return left.severity == right.severity && left.code == right.code
        && left.message == right.message && left.scopeId == right.scopeId
        && left.objectId == right.objectId && left.fieldKey == right.fieldKey
        && left.targetPageId == right.targetPageId;
}

bool stampEquals(const ValidationInputStamp &left,
                 const ValidationInputStamp &right)
{
    const AccessResult<ValidationInputStamp> leftResult =
        normalizeValidationInputStamp(left);
    const AccessResult<ValidationInputStamp> rightResult =
        normalizeValidationInputStamp(right);
    if (!leftResult.ok() || !rightResult.ok())
        return false;
    const ValidationInputStamp &a = leftResult.value();
    const ValidationInputStamp &b = rightResult.value();
    if (!scopeEquals(a.scope, b.scope)
        || a.definitionVersion != b.definitionVersion
        || a.parameter.snapshotId != b.parameter.snapshotId
        || !scopeEquals(a.parameter.scope, b.parameter.scope)
        || a.parameter.revision != b.parameter.revision
        || a.resources.size() != b.resources.size()) {
        return false;
    }
    for (int index = 0; index < a.resources.size(); ++index) {
        const ResourceInputStamp &x = a.resources.at(index);
        const ResourceInputStamp &y = b.resources.at(index);
        if (x.projectId != y.projectId || x.resourceId != y.resourceId
            || x.revision != y.revision
            || x.hasVerifiedFingerprint != y.hasVerifiedFingerprint) {
            return false;
        }
        if (x.hasVerifiedFingerprint
            && (x.fingerprint.status != y.fingerprint.status
                || x.fingerprint.algorithm != y.fingerprint.algorithm
                || x.fingerprint.digest != y.fingerprint.digest
                || x.fingerprint.sizeBytes != y.fingerprint.sizeBytes
                || x.fingerprint.modifiedAt != y.fingerprint.modifiedAt)) {
            return false;
        }
    }
    return true;
}

bool validCompletion(CompletionState state)
{
    return state == CompletionState::NotStarted
        || state == CompletionState::InProgress
        || state == CompletionState::Completed;
}

AccessResult<WorkflowFactsSnapshot> invalidSnapshot(const QString &message)
{
    return AccessResult<WorkflowFactsSnapshot>::failure(
        AccessStatus::InvalidArgument,
        QStringLiteral("workflow.snapshot.invalid"), message);
}

bool stepFactsEqual(const WorkflowStepFacts &left,
                    const WorkflowStepFacts &right)
{
    if (left.stepId != right.stepId
        || left.committedCompletion != right.committedCompletion
        || left.hasValidationFact != right.hasValidationFact
        || left.factRevision != right.factRevision
        || left.updatedAt != right.updatedAt) {
        return false;
    }
    if (!left.hasValidationFact)
        return true;
    const ValidationFact &a = left.validationFact;
    const ValidationFact &b = right.validationFact;
    if (a.outcome != b.outcome || a.validatedAt != b.validatedAt
        || !stampEquals(a.inputStamp, b.inputStamp)
        || a.issues.size() != b.issues.size()) {
        return false;
    }
    for (int index = 0; index < a.issues.size(); ++index) {
        if (!issueEquals(a.issues.at(index), b.issues.at(index)))
            return false;
    }
    return true;
}

} // namespace

AccessResult<WorkflowFactsSnapshot> validateWorkflowFactsSnapshot(
    WorkflowFactsSnapshot snapshot)
{
    if (snapshot.scope.projectId.isEmpty()
        || snapshot.scope.caseId.isEmpty()
        || snapshot.definitionVersion.isEmpty()) {
        return invalidSnapshot(QStringLiteral(
            "snapshot scope and definition version must be present"));
    }

    QSet<QString> stepIds;
    for (WorkflowStepFacts &facts : snapshot.steps) {
        if (facts.stepId.isEmpty() || stepIds.contains(facts.stepId)) {
            return invalidSnapshot(QStringLiteral(
                "snapshot step identifiers must be present and unique"));
        }
        if (facts.factRevision > snapshot.factRevision
            || !validCompletion(facts.committedCompletion)) {
            return invalidSnapshot(QStringLiteral(
                "snapshot step facts exceed the scope revision or contain an invalid state"));
        }
        stepIds.insert(facts.stepId);
        if (facts.hasValidationFact) {
            if (!validationInputStampIsValid(
                    facts.validationFact.inputStamp)
                || !scopeEquals(snapshot.scope,
                                facts.validationFact.inputStamp.scope)
                || facts.validationFact.inputStamp.definitionVersion
                    != snapshot.definitionVersion) {
                return invalidSnapshot(QStringLiteral(
                    "snapshot contains an invalid validation input stamp"));
            }
            facts.validationFact.inputStamp =
                normalizeValidationInputStamp(
                    facts.validationFact.inputStamp).value();
        }
    }
    std::sort(snapshot.steps.begin(), snapshot.steps.end(),
              [](const WorkflowStepFacts &left,
                 const WorkflowStepFacts &right) {
                  return left.stepId < right.stepId;
              });
    return AccessResult<WorkflowFactsSnapshot>::success(snapshot);
}

AccessResult<WorkflowFactsSnapshot> makeWorkflowFactsSnapshot(
    const WorkflowScopeFacts &facts, const QDateTime &capturedAt)
{
    WorkflowFactsSnapshot snapshot;
    snapshot.scope = facts.scope;
    snapshot.definitionVersion = facts.definitionVersion;
    snapshot.factRevision = facts.factRevision;
    snapshot.capturedAt = capturedAt;
    snapshot.steps = facts.steps;
    return validateWorkflowFactsSnapshot(snapshot);
}

WorkflowScopeFacts workflowFactsFromSnapshot(
    const WorkflowFactsSnapshot &snapshot)
{
    WorkflowScopeFacts facts;
    facts.scope = snapshot.scope;
    facts.definitionVersion = snapshot.definitionVersion;
    facts.factRevision = snapshot.factRevision;
    facts.steps = snapshot.steps;
    return facts;
}

bool workflowSnapshotFactsEqual(const WorkflowFactsSnapshot &leftValue,
                                const WorkflowFactsSnapshot &rightValue)
{
    const AccessResult<WorkflowFactsSnapshot> left =
        validateWorkflowFactsSnapshot(leftValue);
    const AccessResult<WorkflowFactsSnapshot> right =
        validateWorkflowFactsSnapshot(rightValue);
    if (!left.ok() || !right.ok())
        return false;
    if (!scopeEquals(left.value().scope, right.value().scope)
        || left.value().definitionVersion != right.value().definitionVersion
        || left.value().factRevision != right.value().factRevision
        || left.value().steps.size() != right.value().steps.size()) {
        return false;
    }
    for (int index = 0; index < left.value().steps.size(); ++index) {
        if (!stepFactsEqual(left.value().steps.at(index),
                            right.value().steps.at(index))) {
            return false;
        }
    }
    return true;
}

} // namespace workflow
} // namespace aerosim
