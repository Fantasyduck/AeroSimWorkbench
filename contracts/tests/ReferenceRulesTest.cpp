#include "TestSupport.h"

#include "aerosim/contracts/References.h"

int main()
{
    aerosim_test::TestContext context;

    const aerosim::StateRef emptyState;
    AEROSIM_CHECK(context, !emptyState.isValid());
    const aerosim::StateRef stateV1{QStringLiteral("project-a"),
                                    QStringLiteral("state-a"), 1};
    const aerosim::StateRef stateV2{QStringLiteral("project-a"),
                                    QStringLiteral("state-a"), 2};
    const aerosim::StateRef otherProject{QStringLiteral("project-b"),
                                         QStringLiteral("state-a"), 1};
    const aerosim::StateRef otherState{QStringLiteral("project-a"),
                                       QStringLiteral("state-b"), 1};
    AEROSIM_CHECK(context, stateV1.isValid());
    AEROSIM_CHECK(context, stateV1.sameIdentity(stateV2));
    AEROSIM_CHECK(context, !stateV1.sameVersion(stateV2));
    AEROSIM_CHECK(context, !stateV1.sameIdentity(otherProject));
    AEROSIM_CHECK(context, !stateV1.sameIdentity(otherState));

    const aerosim::BatchPlanRef planV1{QStringLiteral("project-a"),
                                       QStringLiteral("plan-a"), 1};
    const aerosim::BatchPlanRef planV2{QStringLiteral("project-a"),
                                       QStringLiteral("plan-a"), 2};
    AEROSIM_CHECK(context, planV1.isValid());
    AEROSIM_CHECK(context, planV1.sameIdentity(planV2));
    AEROSIM_CHECK(context, !planV1.sameVersion(planV2));

    const aerosim::BatchRunRef runV1{QStringLiteral("project-a"),
                                     QStringLiteral("run-a"), planV1};
    const aerosim::BatchRunRef runV2{QStringLiteral("project-a"),
                                     QStringLiteral("run-a"), planV2};
    AEROSIM_CHECK(context, runV1.isValid());
    AEROSIM_CHECK(context, runV1.sameIdentity(runV2));
    AEROSIM_CHECK(context, runV1.sameVersion(runV2));
    const aerosim::BatchRunRef wrongPlanProject{
        QStringLiteral("project-a"), QStringLiteral("run-a"),
        {QStringLiteral("project-b"), QStringLiteral("plan-a"), 1}};
    AEROSIM_CHECK(context, !wrongPlanProject.isValid());

    const aerosim::TaskRef batchTask{QStringLiteral("project-a"),
                                     QStringLiteral("task-a"),
                                     QStringLiteral("run-a")};
    const aerosim::TaskRef singleTask{QStringLiteral("project-a"),
                                      QStringLiteral("task-a"), QString()};
    AEROSIM_CHECK(context, batchTask.isValid());
    AEROSIM_CHECK(context, singleTask.isValid());
    AEROSIM_CHECK(context, batchTask.sameIdentity(singleTask));
    AEROSIM_CHECK(context, batchTask.sameVersion(singleTask));
    const aerosim::TaskRef otherTask{QStringLiteral("project-a"),
                                     QStringLiteral("task-b"), QString()};
    AEROSIM_CHECK(context, !batchTask.sameIdentity(otherTask));

    const aerosim::AttemptRef attemptA{batchTask, QStringLiteral("attempt-a")};
    const aerosim::AttemptRef attemptB{singleTask, QStringLiteral("attempt-b")};
    const aerosim::AttemptRef emptyAttempt{batchTask, QString()};
    AEROSIM_CHECK(context, attemptA.isValid());
    AEROSIM_CHECK(context, !attemptA.sameIdentity(attemptB));
    AEROSIM_CHECK(context, !emptyAttempt.isValid());

    const aerosim::StateRef unspecifiedState{
        QStringLiteral("project-a"), QStringLiteral("state-a"), 0};
    const aerosim::BatchPlanRef unspecifiedPlan{
        QStringLiteral("project-a"), QStringLiteral("plan-a"), 0};
    AEROSIM_CHECK(context, !unspecifiedState.isValid());
    AEROSIM_CHECK(context, !unspecifiedPlan.isValid());

    return context.result();
}
