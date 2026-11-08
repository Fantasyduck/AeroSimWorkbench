#pragma once

#include "aerosim/contracts/TaskPorts.h"
#include "aerosim/workflow/WorkflowDefinition.h"
#include "aerosim/workflow/WorkflowFacts.h"
#include "aerosim/workflow/WorkflowStateDeriver.h"
#include "aerosim/workflow/ValidationTypes.h"
#include "aerosim/workflow/WorkflowSnapshot.h"

#include <memory>

namespace aerosim {
namespace workflow {

// Qt Core facade. Mutable facts are owned and committed on this object's
// thread; worker validation results must return through a queued value message.
class WorkflowService final : public IWorkflowService {
    Q_OBJECT
public:
    explicit WorkflowService(QObject *parent = nullptr);
    ~WorkflowService() override;

    AccessResult<WorkflowViewV2> viewV2(const ScopeRef &scope) const override;
    ValidationResult requestRun(const RunRequest &request) override;

    AccessResult<WorkflowDefinition>
    setDefinition(const WorkflowDefinition &definition);
    AccessResult<quint64> setCurrentInput(
        const ScopeRef &scope, const QString &stepId,
        const ValidationInputStamp &inputStamp);
    AccessResult<quint64> beginEditing(
        const ScopeRef &scope, const QString &stepId,
        const QString &sessionId);
    AccessResult<quint64> cancelEditing(
        const ScopeRef &scope, const QString &stepId);
    AccessResult<quint64> failEditingSave(
        const ScopeRef &scope, const QString &stepId);
    AccessResult<quint64> saveEditing(
        const ScopeRef &scope, const QString &stepId);
    AccessResult<quint64> acceptValidation(
        const ScopeRef &scope, const QString &stepId,
        ValidationOutcome outcome, const ValidationInputStamp &inputStamp,
        const QVector<ValidationIssue> &issues = QVector<ValidationIssue>(),
        const QDateTime &validatedAt = QDateTime::currentDateTimeUtc());
    AccessResult<WorkflowScopeFacts> facts(const ScopeRef &scope) const;
    AccessResult<PendingValidation> beginValidation(const ScopeRef &, const QString &, const QString &);
    void cancelValidation(const ScopeRef &, const QString &);
    void destroyScope(const ScopeRef &);
    void shutdown();
    AccessResult<WorkflowFactsSnapshot> snapshot(const ScopeRef &) const;
    AccessResult<quint64> restoreSnapshot(const WorkflowFactsSnapshot &);

public slots:
    void onTaskEvent(const TaskEvent &event) override;
    void onValidationCompleted(aerosim::workflow::ValidationCompletion completion);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace workflow
} // namespace aerosim
