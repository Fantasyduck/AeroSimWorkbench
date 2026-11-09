#include "TestSupport.h"

#include "aerosim/contracts/EnumTokens.h"

#include <limits>
#include <type_traits>

namespace {

aerosim::ResourceRefV2 verifiedResource(const QString &projectId,
                                        const QString &resourceId,
                                        aerosim::ResourceType type,
                                        quint64 revision)
{
    aerosim::ResourceRefV2 resource;
    resource.projectId = projectId;
    resource.resourceId = resourceId;
    resource.revision = revision;
    resource.type = type;
    resource.location = aerosim::ResourceLocation::projectRelative(
        QStringLiteral("snapshots/snapshot.json"));
    resource.fingerprint.status = aerosim::FingerprintStatus::Verified;
    resource.fingerprint.algorithm = QStringLiteral("sha256");
    resource.fingerprint.digest = QStringLiteral("abcdef");
    return resource;
}

aerosim::StateRef pinnedState(quint64 revision = 2)
{
    aerosim::StateRef state;
    state.projectId = QStringLiteral("project-a");
    state.stateId = QStringLiteral("state-a");
    state.revision = revision;
    return state;
}

aerosim::BatchPlanItem planItem(const QString &itemId, const QString &stateId,
                                quint32 order)
{
    aerosim::BatchPlanItem item;
    item.itemId = itemId;
    item.order = order;
    aerosim::StateRef state;
    state.projectId = QStringLiteral("project-a");
    state.stateId = stateId;
    state.revision = 1;
    item.state = state;
    return item;
}

aerosim::BatchPlan makePlan(quint64 revision)
{
    aerosim::BatchPlan plan;
    plan.ref.projectId = QStringLiteral("project-a");
    plan.ref.planId = QStringLiteral("plan-a");
    plan.ref.revision = revision;
    plan.name = QStringLiteral("baseline sweep");
    plan.schemaVersion = 1;
    plan.items.append(planItem(QStringLiteral("item-1"), QStringLiteral("state-a"), 1));
    plan.items.append(planItem(QStringLiteral("item-2"), QStringLiteral("state-b"), 2));
    return plan;
}

void checkRevisionRule(aerosim_test::TestContext &context)
{
    AEROSIM_CHECK(context, aerosim::kInitialRevision == 1);
    AEROSIM_CHECK(context, aerosim::nextRevision(0) == 1);
    AEROSIM_CHECK(context, aerosim::nextRevision(1) == 2);
    AEROSIM_CHECK(context, aerosim::nextRevision(7) == 8);
}

void checkParameterValueAndRange(aerosim_test::TestContext &context)
{
    const aerosim::ParameterValue unspecified;
    AEROSIM_CHECK(context, !unspecified.isValid());
    AEROSIM_CHECK(context, !unspecified.isNumeric());

    const aerosim::ParameterValue mach =
        aerosim::ParameterValue::real(5.0, QStringLiteral("1"));
    AEROSIM_CHECK(context, mach.isValid());
    AEROSIM_CHECK(context, mach.isNumeric());
    AEROSIM_CHECK(context, mach.unit == QStringLiteral("1"));

    AEROSIM_CHECK(context, aerosim::ParameterValue::boolean(true).isValid());
    AEROSIM_CHECK(context, !aerosim::ParameterValue::text(QString()).isValid());
    AEROSIM_CHECK(context,
        aerosim::ParameterValue::enumeration(QStringLiteral("laminar")).isValid());

    aerosim::ParameterValue infinite = aerosim::ParameterValue::real(1.0);
    infinite.realValue = std::numeric_limits<double>::infinity();
    AEROSIM_CHECK(context, !infinite.isValid());

    const aerosim::ParameterRange bounded = aerosim::ParameterRange::between(0.0, 10.0);
    AEROSIM_CHECK(context, bounded.isValid());
    AEROSIM_CHECK(context, bounded.contains(5.0));
    AEROSIM_CHECK(context, !bounded.contains(10.5));
    AEROSIM_CHECK(context, !aerosim::ParameterRange::between(10.0, 0.0).isValid());
    AEROSIM_CHECK(context, aerosim::ParameterRange::atLeast(0.0).contains(0.0));
    AEROSIM_CHECK(context, !aerosim::ParameterRange::atMost(1.0).contains(2.0));
}

void checkParameterEntries(aerosim_test::TestContext &context)
{
    aerosim::ParameterEntry entry =
        aerosim::ParameterEntry::requiredReal(QStringLiteral("mach"), 5.0,
                                             QStringLiteral("1"));
    entry.range = aerosim::ParameterRange::between(0.0, 25.0);
    AEROSIM_CHECK(context, entry.isValid());
    AEROSIM_CHECK(context, entry.required);
    AEROSIM_CHECK(context, entry.isApplicable());
    AEROSIM_CHECK(context, entry.valueWithinRange());

    entry.value = aerosim::ParameterValue::real(40.0, QStringLiteral("1"));
    AEROSIM_CHECK(context, !entry.valueWithinRange());

    entry.applicabilityRule = QStringLiteral("compressible_only");
    AEROSIM_CHECK(context, !entry.isApplicable());

    aerosim::ParameterEntry empty;
    AEROSIM_CHECK(context, !empty.isValid());

    aerosim::ParameterCategoryConfig governingEquations;
    governingEquations.category = aerosim::ParameterCategory::GoverningEquations;
    governingEquations.entries.append(aerosim::ParameterEntry::requiredReal(
        QStringLiteral("mach"), 5.0, QStringLiteral("1")));
    governingEquations.entries.append(
        aerosim::ParameterEntry::requiredBoolean(QStringLiteral("viscous"), true));
    AEROSIM_CHECK(context, governingEquations.isValid());
    AEROSIM_CHECK(context, governingEquations.hasUniqueKeys());
    AEROSIM_CHECK(context, governingEquations.find(QStringLiteral("viscous")) != nullptr);
    AEROSIM_CHECK(context, governingEquations.find(QStringLiteral("missing")) == nullptr);

    governingEquations.entries.append(
        aerosim::ParameterEntry::requiredBoolean(QStringLiteral("viscous"), false));
    AEROSIM_CHECK(context, !governingEquations.hasUniqueKeys());
    AEROSIM_CHECK(context, !governingEquations.isValid());

    // Solver-specific configuration stays an open, typed boundary instead of
    // inventing unconfirmed fields.
    aerosim::SolverConfigPayload payload;
    AEROSIM_CHECK(context, !payload.isValid());
    payload.solverId = QStringLiteral("structured-density-based");
    payload.schemaId = QStringLiteral("vendor.turbulence.sa");
    payload.schemaVersion = 3;
    payload.entries.append(aerosim::ParameterEntry::requiredReal(
        QStringLiteral("saCoefficient"), 2.0, QString()));
    AEROSIM_CHECK(context, payload.isValid());
}

void checkParameterSnapshot(aerosim_test::TestContext &context)
{
    aerosim::ParameterSnapshot snapshot;
    AEROSIM_CHECK(context, !snapshot.isIdentified());
    AEROSIM_CHECK(context, !snapshot.isValid());

    snapshot.ref.snapshotId = QStringLiteral("snapshot-1");
    snapshot.ref.revision = 1;
    snapshot.state = pinnedState();
    snapshot.ref.scope.projectId = snapshot.state.projectId;
    snapshot.ref.scope.caseId = snapshot.state.stateId;
    snapshot.schemaVersion = 1;
    snapshot.fingerprint.status = aerosim::FingerprintStatus::Computed;
    snapshot.fingerprint.algorithm = QStringLiteral("sha256");
    snapshot.fingerprint.digest = QStringLiteral("deadbeef");
    aerosim::ParameterCategoryConfig category;
    category.category = aerosim::ParameterCategory::ReferenceValues;
    category.entries.append(aerosim::ParameterEntry::requiredReal(
        QStringLiteral("referenceArea"), 1.0, QStringLiteral("m^2")));
    snapshot.categories.append(category);

    AEROSIM_CHECK(context, snapshot.isIdentified());
    AEROSIM_CHECK(context, snapshot.isPinned());
    AEROSIM_CHECK(context, snapshot.scopeMirrorsState());
    AEROSIM_CHECK(context, snapshot.isValid());
    AEROSIM_CHECK(context,
        snapshot.category(aerosim::ParameterCategory::ReferenceValues) != nullptr);
    AEROSIM_CHECK(context,
        snapshot.entry(aerosim::ParameterCategory::ReferenceValues,
                       QStringLiteral("referenceArea")) != nullptr);
    AEROSIM_CHECK(context,
        snapshot.category(aerosim::ParameterCategory::MeshMotion) == nullptr);

    // A conflicting legacy caseId must not be silently accepted.
    snapshot.ref.scope.caseId = QStringLiteral("state-other");
    AEROSIM_CHECK(context, !snapshot.scopeMirrorsState());
    AEROSIM_CHECK(context, !snapshot.isValid());
    snapshot.ref.scope.caseId.clear();
    AEROSIM_CHECK(context, snapshot.scopeMirrorsState());

    // revision 0 means unspecified/legacy and is not a pinned snapshot.
    snapshot.ref.revision = 0;
    AEROSIM_CHECK(context, !snapshot.isIdentified());
    AEROSIM_CHECK(context, !snapshot.isValid());
    snapshot.ref.revision = 1;

    snapshot.state.revision = 0;
    AEROSIM_CHECK(context, !snapshot.isPinned());
    AEROSIM_CHECK(context, !snapshot.isValid());
}

void checkStateConfig(aerosim_test::TestContext &context)
{
    aerosim::StateConfig state;
    AEROSIM_CHECK(context, !state.isValid());
    AEROSIM_CHECK(context, !state.isComputable());

    state.ref = pinnedState(1);
    state.name = QStringLiteral("Ma5 h5000");
    state.solverId = QStringLiteral("structured-density-based");
    AEROSIM_CHECK(context, state.isValid());
    // Editable but not yet runnable: no pinned mesh or snapshot.
    AEROSIM_CHECK(context, !state.isComputable());

    state.computationMesh.projectId = QStringLiteral("project-a");
    state.computationMesh.meshId = QStringLiteral("mesh-a");
    state.computationMesh.revision = 3;
    state.parameterSnapshot.snapshotId = QStringLiteral("snapshot-1");
    state.parameterSnapshot.revision = 2;
    state.parameterSnapshot.scope.projectId = QStringLiteral("project-a");
    state.parameterSnapshot.scope.caseId = state.ref.stateId;
    AEROSIM_CHECK(context, state.isComputable());

    // Unspecified mesh revision must not be used as a computation input.
    state.computationMesh.revision = 0;
    AEROSIM_CHECK(context, !state.isComputable());
}

void checkBatchPlanHistory(aerosim_test::TestContext &context)
{
    aerosim::BatchPlanHistory history;
    history.projectId = QStringLiteral("project-a");
    history.planId = QStringLiteral("plan-a");
    AEROSIM_CHECK(context, !history.isValid());
    AEROSIM_CHECK(context, history.nextRevision() == 1);

    AEROSIM_CHECK(context, history.appendRevision(makePlan(1)));
    AEROSIM_CHECK(context, history.appendRevision(makePlan(2)));
    AEROSIM_CHECK(context, history.isValid());
    AEROSIM_CHECK(context, history.revisions.size() == 2);
    AEROSIM_CHECK(context, history.nextRevision() == 3);
    AEROSIM_CHECK(context, history.latest() != nullptr);
    AEROSIM_CHECK(context, history.latest()->ref.revision == 2);
    AEROSIM_CHECK(context, history.findRevision(1) != nullptr);
    AEROSIM_CHECK(context, history.findRevision(99) == nullptr);

    // Overwriting an existing revision is rejected: history is append-only.
    AEROSIM_CHECK(context, !history.appendRevision(makePlan(1)));
    // Skipping a revision is rejected.
    AEROSIM_CHECK(context, !history.appendRevision(makePlan(4)));
    AEROSIM_CHECK(context, history.revisions.size() == 2);

    // A different plan identity must not be mixed into this history.
    aerosim::BatchPlan alien = makePlan(3);
    alien.ref.planId = QStringLiteral("plan-b");
    AEROSIM_CHECK(context, !history.appendRevision(alien));

    // Earlier revisions stay readable after appending a new one.
    AEROSIM_CHECK(context, history.appendRevision(makePlan(3)));
    AEROSIM_CHECK(context, history.findRevision(1)->items.size() == 2);
    AEROSIM_CHECK(context, history.findRevision(2) != nullptr);
}

void checkExecutionSnapshot(aerosim_test::TestContext &context)
{
    const aerosim::BatchPlan plan = makePlan(2);

    aerosim::BatchPlanExecutionSnapshot snapshot;
    AEROSIM_CHECK(context, !snapshot.isValid());

    snapshot.resource = verifiedResource(QStringLiteral("project-a"),
        QStringLiteral("snapshot-resource"),
        aerosim::ResourceType::BatchPlanSnapshot, 1);
    snapshot.sourcePlan = plan.ref;
    snapshot.items = plan.items;
    snapshot.executionPolicy = QStringLiteral("sequential");
    snapshot.schemaVersion = 1;
    AEROSIM_CHECK(context, snapshot.isValid());
    AEROSIM_CHECK(context, snapshot.matches(plan));

    // The snapshot must be typed as a batch plan snapshot resource.
    snapshot.resource.type = aerosim::ResourceType::Result;
    AEROSIM_CHECK(context, !snapshot.isValid());
    snapshot.resource.type = aerosim::ResourceType::BatchPlanSnapshot;

    // Later plan edits must not match the pinned snapshot.
    aerosim::BatchPlan edited = makePlan(3);
    edited.items[0].order = 5;
    AEROSIM_CHECK(context, !snapshot.matches(edited));
    AEROSIM_CHECK(context, snapshot.matches(plan));
}

void checkValueTypes(aerosim_test::TestContext &context)
{
    AEROSIM_CHECK(context, std::is_copy_constructible<aerosim::ParameterSnapshot>::value);
    AEROSIM_CHECK(context, std::is_copy_assignable<aerosim::StateConfig>::value);
    AEROSIM_CHECK(context, std::is_copy_constructible<aerosim::BatchPlan>::value);
    AEROSIM_CHECK(context,
        std::is_copy_constructible<aerosim::BatchPlanExecutionSnapshot>::value);
}

void checkTokens(aerosim_test::TestContext &context)
{
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::ParameterCategory::MeshMotion)
            == QStringLiteral("mesh_motion"));
    AEROSIM_CHECK(context,
        aerosim::parameterCategoryFromToken(QStringLiteral("initialization")).value()
            == aerosim::ParameterCategory::Initialization);
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::ParameterValueKind::Enumeration)
            == QStringLiteral("enumeration"));
    AEROSIM_CHECK(context,
        aerosim::parameterValueKindFromToken(QStringLiteral("real")).value()
            == aerosim::ParameterValueKind::Real);
    AEROSIM_CHECK(context,
        aerosim::enumToken(static_cast<aerosim::ParameterCategory>(-1)).isEmpty());
    AEROSIM_CHECK(context,
        aerosim::parameterCategoryFromToken(QStringLiteral("nope")).status()
            == aerosim::AccessStatus::InvalidArgument);
}

} // namespace

int main()
{
    aerosim_test::TestContext context;
    checkRevisionRule(context);
    checkParameterValueAndRange(context);
    checkParameterEntries(context);
    checkParameterSnapshot(context);
    checkStateConfig(context);
    checkBatchPlanHistory(context);
    checkExecutionSnapshot(context);
    checkValueTypes(context);
    checkTokens(context);
    return context.result();
}
