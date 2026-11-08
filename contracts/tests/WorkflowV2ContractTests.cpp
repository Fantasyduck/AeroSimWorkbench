#include "TestSupport.h"

#include "aerosim/contracts/EnumTokens.h"
#include "aerosim/contracts/TaskPorts.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QMetaType>
#include <QObject>

#include <type_traits>

namespace {

// A minimal implementation that relies on the default viewV2() contract. It is
// used to prove that viewV2() is required and returns Unsupported until a real
// workflow implementation overrides it.
class MinimalWorkflow final : public aerosim::IWorkflowService {
public:
    using aerosim::IWorkflowService::IWorkflowService;

    aerosim::ValidationResult requestRun(const aerosim::RunRequest &) override
    {
        return aerosim::ValidationResult();
    }

    void onTaskEvent(const aerosim::TaskEvent &) override {}

    void publish(aerosim::WorkflowViewV2 view)
    {
        emit workflowViewV2Changed(view);
    }
};

void checkDefaults(aerosim_test::TestContext &context)
{
    const aerosim::WorkflowStepViewV2 step;
    AEROSIM_CHECK(context, step.applicable);
    AEROSIM_CHECK(context,
        step.completion == aerosim::CompletionState::NotStarted);
    AEROSIM_CHECK(context,
        step.validation == aerosim::ValidationState::NotValidated);
    AEROSIM_CHECK(context, step.blockers.isEmpty());
    AEROSIM_CHECK(context, step.factRevision == 0);

    const aerosim::WorkflowViewV2 view;
    AEROSIM_CHECK(context, view.definitionVersion.isEmpty());
    AEROSIM_CHECK(context, view.viewRevision == 0);
    AEROSIM_CHECK(context, view.steps.isEmpty());
    AEROSIM_CHECK(context, view.actions.isEmpty());
    AEROSIM_CHECK(context, view.issues.isEmpty());

    static_assert(std::is_copy_constructible<aerosim::WorkflowViewV2>::value,
        "WorkflowViewV2 must be copy constructible");
    static_assert(std::is_copy_assignable<aerosim::WorkflowViewV2>::value,
        "WorkflowViewV2 must be copy assignable");
}

void checkUnsupportedDefault(aerosim_test::TestContext &context)
{
    MinimalWorkflow workflow;
    const aerosim::AccessResult<aerosim::WorkflowViewV2> result =
        workflow.viewV2(aerosim::ScopeRef());
    AEROSIM_CHECK(context, !result.ok());
    AEROSIM_CHECK(context, !result.hasValue());
    AEROSIM_CHECK(context,
        result.status() == aerosim::AccessStatus::Unsupported);
    AEROSIM_CHECK(context,
        result.errorCode() == QStringLiteral("workflow.v2.unsupported"));
    AEROSIM_CHECK(context,
        result.message()
            == QStringLiteral("implementation does not provide Workflow V2"));
}

void checkTokens(aerosim_test::TestContext &context)
{
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::CompletionState::Completed)
            == QStringLiteral("completed"));
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::ValidationState::Stale)
            == QStringLiteral("stale"));
    AEROSIM_CHECK(context,
        aerosim::completionStateFromToken(QStringLiteral("in_progress")).value()
            == aerosim::CompletionState::InProgress);
    AEROSIM_CHECK(context,
        aerosim::validationStateFromToken(QStringLiteral("valid")).value()
            == aerosim::ValidationState::Valid);
    AEROSIM_CHECK(context,
        aerosim::enumToken(static_cast<aerosim::CompletionState>(-1)).isEmpty());
    AEROSIM_CHECK(context,
        aerosim::enumToken(static_cast<aerosim::ValidationState>(-1)).isEmpty());
    AEROSIM_CHECK(context,
        aerosim::validationStateFromToken(QStringLiteral("unknown")).status()
            == aerosim::AccessStatus::InvalidArgument);
}

void checkQueuedDelivery(aerosim_test::TestContext &context)
{
    aerosim::registerContractMetaTypes();
    AEROSIM_CHECK(context,
        QMetaType::type("aerosim::WorkflowViewV2") != QMetaType::UnknownType);

    MinimalWorkflow workflow;
    QObject receiver;
    bool delivered = false;
    quint64 deliveredRevision = 0;
    QObject::connect(&workflow, &aerosim::IWorkflowService::workflowViewV2Changed,
        &receiver,
        [&delivered, &deliveredRevision](aerosim::WorkflowViewV2 view) {
            delivered = true;
            deliveredRevision = view.viewRevision;
        },
        Qt::QueuedConnection);

    aerosim::WorkflowViewV2 view;
    view.viewRevision = 17;
    workflow.publish(view);
    AEROSIM_CHECK(context, !delivered);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    AEROSIM_CHECK(context, delivered);
    AEROSIM_CHECK(context, deliveredRevision == 17);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application);

    aerosim_test::TestContext context;
    checkDefaults(context);
    checkUnsupportedDefault(context);
    checkTokens(context);
    checkQueuedDelivery(context);
    return context.result();
}
