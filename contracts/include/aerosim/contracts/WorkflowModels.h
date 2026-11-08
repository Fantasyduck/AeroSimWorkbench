#pragma once

#include "aerosim/contracts/References.h"

#include <QVector>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace aerosim {

enum class IssueSeverity { Info, Warning, Error };

struct ValidationIssue {
    IssueSeverity severity = IssueSeverity::Error;
    QString code;
    QString message;
    QString scopeId;
    QString objectId;
    QString fieldKey;
    QString targetPageId;
};

struct ValidationResult {
    bool allowed = false;
    QVector<ValidationIssue> issues;
};

enum class CompletionState { NotStarted, InProgress, Completed };

enum class ValidationState { NotValidated, Valid, Invalid, Stale };

struct DependencyBlockerView {
    QString dependencyStepId;
    QString code;
    QString message;
    QString targetPageId;
};

struct WorkflowActionView {
    QString actionId;
    bool enabled = false;
    QString reason;
    QString targetPageId;
    QString scopeId;
    QString objectId;
    QString fieldKey;
};

struct WorkflowStepViewV2 {
    QString stepId;
    ScopeRef scope;
    bool applicable = true;
    QString applicabilityRuleId;
    CompletionState completion = CompletionState::NotStarted;
    ValidationState validation = ValidationState::NotValidated;
    QVector<DependencyBlockerView> blockers;
    QString summary;
    QString targetPageId;
    quint64 factRevision = 0;
};

struct WorkflowViewV2 {
    ScopeRef scope;
    QString definitionVersion;
    quint64 viewRevision = 0;
    QVector<WorkflowStepViewV2> steps;
    QVector<WorkflowActionView> actions;
    QVector<ValidationIssue> issues;
    QString nextSuggestion;
};

struct RunRequest {
    ScopeRef scope;
    ParameterSnapshotRef inputSnapshot;
    QString batchPlanId;
    quint64 batchPlanVersion = 0;
};

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::WorkflowViewV2)
