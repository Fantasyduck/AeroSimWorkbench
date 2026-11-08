#pragma once

#include "aerosim/workflow/WorkflowFacts.h"

#include <QMetaType>

namespace aerosim {
namespace workflow {

struct PendingValidation {
    QString requestId;
    ScopeRef scope;
    QString stepId;
    quint64 generation = 0;
    ValidationInputStamp inputStamp;
};

struct ValidationCompletion {
    QString requestId;
    ScopeRef scope;
    QString stepId;
    quint64 generation = 0;
    ValidationInputStamp inputStamp;
    ValidationOutcome outcome = ValidationOutcome::Failed;
    QVector<ValidationIssue> issues;
    QDateTime completedAt;
};

void registerWorkflowMetaTypes();

} // namespace workflow
} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::workflow::ValidationCompletion)
