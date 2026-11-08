#include "WorkflowTestSupport.h"

#include "aerosim/workflow/ValidationStampMatcher.h"

namespace {

aerosim::workflow::ValidationInputStamp makeStamp()
{
    aerosim::workflow::ValidationInputStamp stamp;
    stamp.scope.projectId = QStringLiteral("project");
    stamp.scope.caseId = QStringLiteral("case");
    stamp.scope.objectId = QStringLiteral("object");
    stamp.definitionVersion = QStringLiteral("workflow-v2");
    stamp.parameter.snapshotId = QStringLiteral("parameters");
    stamp.parameter.scope = stamp.scope;
    stamp.parameter.revision = 7;

    aerosim::workflow::ResourceInputStamp mesh;
    mesh.projectId = QStringLiteral("project");
    mesh.resourceId = QStringLiteral("mesh");
    mesh.revision = 3;
    mesh.hasVerifiedFingerprint = true;
    mesh.fingerprint.status = aerosim::FingerprintStatus::Verified;
    mesh.fingerprint.algorithm = QStringLiteral("sha256");
    mesh.fingerprint.digest = QStringLiteral("mesh-digest");
    mesh.fingerprint.sizeBytes = 1024;
    mesh.fingerprint.modifiedAt = QDateTime::fromMSecsSinceEpoch(1000);
    stamp.resources.append(mesh);

    aerosim::workflow::ResourceInputStamp material;
    material.projectId = QStringLiteral("project");
    material.resourceId = QStringLiteral("material");
    material.revision = 2;
    stamp.resources.append(material);
    return stamp;
}

bool matches(const aerosim::workflow::ValidationInputStamp &left,
             const aerosim::workflow::ValidationInputStamp &right)
{
    const aerosim::AccessResult<bool> result =
        aerosim::workflow::validationInputStampsMatch(left, right);
    return result.ok() && result.value();
}

} // namespace

int main()
{
    aerosim_workflow_test::TestContext context;
    const aerosim::workflow::ValidationInputStamp original = makeStamp();
    AEROSIM_WORKFLOW_CHECK(context,
        aerosim::workflow::validationInputStampIsValid(original));
    AEROSIM_WORKFLOW_CHECK(context, matches(original, original));

    aerosim::workflow::ValidationInputStamp changed = original;
    changed.scope.caseId = QStringLiteral("other-case");
    changed.parameter.scope = changed.scope;
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));

    changed = original;
    changed.definitionVersion = QStringLiteral("workflow-v3");
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));

    changed = original;
    changed.parameter.snapshotId = QStringLiteral("other-parameters");
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));
    changed = original;
    changed.parameter.scope.objectId = QStringLiteral("other-object");
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));
    changed = original;
    changed.parameter.revision += 1;
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));

    changed = original;
    changed.resources[0].resourceId = QStringLiteral("other-mesh");
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));
    changed = original;
    changed.resources[0].revision += 1;
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));

    changed = original;
    changed.resources[0].fingerprint.algorithm = QStringLiteral("sha512");
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));
    changed = original;
    changed.resources[0].fingerprint.digest = QStringLiteral("other-digest");
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));
    changed = original;
    changed.resources[0].fingerprint.sizeBytes += 1;
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));

    changed = original;
    changed.resources[0].fingerprint.modifiedAt =
        QDateTime::fromMSecsSinceEpoch(999999);
    AEROSIM_WORKFLOW_CHECK(context, matches(original, changed));

    changed = original;
    changed.resources[0].revision += 1;
    AEROSIM_WORKFLOW_CHECK(context,
        changed.resources[0].fingerprint.digest
            == original.resources[0].fingerprint.digest);
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));

    changed = original;
    changed.resources[0].hasVerifiedFingerprint = false;
    changed.resources[0].fingerprint = aerosim::ContentFingerprint();
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));

    changed = original;
    changed.resources[1].hasVerifiedFingerprint = false;
    changed.resources[1].fingerprint = aerosim::ContentFingerprint();
    AEROSIM_WORKFLOW_CHECK(context, matches(original, changed));

    changed = original;
    const aerosim::workflow::ResourceInputStamp first = changed.resources.takeFirst();
    changed.resources.append(first);
    AEROSIM_WORKFLOW_CHECK(context, matches(original, changed));

    changed = original;
    changed.resources.removeLast();
    AEROSIM_WORKFLOW_CHECK(context, !matches(original, changed));
    changed = original;
    changed.resources.append(changed.resources.first());
    AEROSIM_WORKFLOW_CHECK(context,
        !aerosim::workflow::normalizeValidationInputStamp(changed).ok());

    changed = original;
    changed.parameter.revision = 0;
    AEROSIM_WORKFLOW_CHECK(context,
        !aerosim::workflow::normalizeValidationInputStamp(changed).ok());

    quint64 operationCount = 0;
    const aerosim::AccessResult<bool> counted =
        aerosim::workflow::validationInputStampsMatch(
            original, original, &operationCount);
    AEROSIM_WORKFLOW_CHECK(context, counted.ok() && counted.value());
    AEROSIM_WORKFLOW_CHECK(context,
        operationCount == static_cast<quint64>(original.resources.size() + 1));

    return context.result();
}
