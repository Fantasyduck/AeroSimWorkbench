#pragma once

#include "aerosim/contracts/ResourceModels.h"
#include "aerosim/contracts/WorkflowModels.h"

#include <QDateTime>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace aerosim {
namespace workflow {

struct ResourceInputStamp {
    QString projectId;
    QString resourceId;
    quint64 revision = 0;
    bool hasVerifiedFingerprint = false;
    ContentFingerprint fingerprint;
};

struct ValidationInputStamp {
    ScopeRef scope;
    QString definitionVersion;
    ParameterSnapshotRef parameter;
    QVector<ResourceInputStamp> resources;
};

enum class ValidationOutcome { Passed, Failed };

struct ValidationFact {
    ValidationOutcome outcome = ValidationOutcome::Failed;
    QVector<ValidationIssue> issues;
    ValidationInputStamp inputStamp;
    QDateTime validatedAt;
};

struct WorkflowStepFacts {
    QString stepId;
    CompletionState committedCompletion = CompletionState::NotStarted;
    bool hasValidationFact = false;
    ValidationFact validationFact;
    quint64 factRevision = 0;
    QDateTime updatedAt;
};

struct WorkflowScopeFacts {
    ScopeRef scope;
    QString definitionVersion;
    quint64 factRevision = 0;
    QVector<WorkflowStepFacts> steps;
};

struct EditingDraftOverlay {
    ScopeRef scope;
    QString stepId;
    quint64 baseFactRevision = 0;
    QString sessionId;
};

} // namespace workflow
} // namespace aerosim
