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

enum class StepState { Pending, NeedsValidation, Complete, Blocked };

struct WorkflowStepView {
    QString stepId;
    QString scopeId;
    StepState state = StepState::Pending;
    QString summary;
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

struct WorkflowView {
    ScopeRef scope;
    quint64 viewRevision = 0;
    QVector<WorkflowStepView> steps;
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

Q_DECLARE_METATYPE(aerosim::WorkflowView)
