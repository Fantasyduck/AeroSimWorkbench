#pragma once

#include "aerosim/contracts/AccessResult.h"
#include "aerosim/workflow/WorkflowFacts.h"

namespace aerosim {
namespace workflow {

// Storage-neutral committed facts. Runtime drafts, pending validation,
// derived blockers/actions, and any JSON/file representation are excluded.
struct WorkflowFactsSnapshot {
    ScopeRef scope;
    QString definitionVersion;
    quint64 factRevision = 0;
    QDateTime capturedAt;
    QVector<WorkflowStepFacts> steps;
};

AccessResult<WorkflowFactsSnapshot> validateWorkflowFactsSnapshot(
    WorkflowFactsSnapshot snapshot);
AccessResult<WorkflowFactsSnapshot> makeWorkflowFactsSnapshot(
    const WorkflowScopeFacts &facts,
    const QDateTime &capturedAt = QDateTime::currentDateTimeUtc());
WorkflowScopeFacts workflowFactsFromSnapshot(
    const WorkflowFactsSnapshot &snapshot);
bool workflowSnapshotFactsEqual(const WorkflowFactsSnapshot &left,
                                const WorkflowFactsSnapshot &right);

} // namespace workflow
} // namespace aerosim
