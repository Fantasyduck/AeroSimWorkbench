#pragma once

#include "aerosim/workflow/WorkflowDefinition.h"
#include "aerosim/workflow/WorkflowFacts.h"
#include "aerosim/workflow/ActionAuthorizer.h"

namespace aerosim {
namespace workflow {

struct WorkflowStepCurrentInput {
    QString stepId;
    ValidationInputStamp inputStamp;
};

struct WorkflowDerivationContext {
    ScopeRef scope;
    WorkflowDefinition definition;
    WorkflowScopeFacts facts;
    QVector<WorkflowStepCurrentInput> currentInputs;
    QVector<EditingDraftOverlay> drafts;
    quint64 viewRevision = 0;
};

WorkflowViewV2 deriveWorkflowView(const WorkflowDerivationContext &context,
                                  quint64 *operationCount = nullptr);

ActionAuthorization authorizeWorkflowAction(
    const WorkflowDerivationContext &context, const QString &actionId);

} // namespace workflow
} // namespace aerosim
