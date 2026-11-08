#pragma once

#include "aerosim/contracts/AccessResult.h"
#include "aerosim/workflow/ValidationTypes.h"

namespace aerosim {
namespace workflow {

class ValidationCoordinator {
public:
    AccessResult<PendingValidation> begin(
        const QString &requestId, const ScopeRef &scope,
        const QString &stepId, const ValidationInputStamp &inputStamp);
    bool accept(const ValidationCompletion &completion,
                const ValidationInputStamp &currentInputStamp);
    void cancel(const ScopeRef &scope, const QString &stepId);
    void destroyScope(const ScopeRef &scope);
    void shutdown();
    bool isShutdown() const { return shutdown_; }

private:
    QVector<PendingValidation> pending_;
    quint64 nextGeneration_ = 1;
    bool shutdown_ = false;
};

} // namespace workflow
} // namespace aerosim
