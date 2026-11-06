#include "TestSupport.h"

#include "aerosim/contracts/ResourceModels.h"

int main()
{
    aerosim_test::TestContext context;

    const aerosim::ResourceLocation relative =
        aerosim::ResourceLocation::projectRelative(
            QStringLiteral("mesh/original/model.cgns"));
    AEROSIM_CHECK(context, relative.isValid());
    AEROSIM_CHECK(context,
        !aerosim::ResourceLocation::projectRelative(QString()).isValid());
    AEROSIM_CHECK(context,
        !aerosim::ResourceLocation::projectRelative(
            QStringLiteral("../outside.cgns")).isValid());
    AEROSIM_CHECK(context,
        !aerosim::ResourceLocation::projectRelative(
            QStringLiteral("mesh/../outside.cgns")).isValid());
    AEROSIM_CHECK(context,
        !aerosim::ResourceLocation::projectRelative(
            QStringLiteral("mesh//model.cgns")).isValid());
    AEROSIM_CHECK(context,
        !aerosim::ResourceLocation::projectRelative(
            QStringLiteral("mesh\\model.cgns")).isValid());
    AEROSIM_CHECK(context,
        !aerosim::ResourceLocation::projectRelative(
            QStringLiteral("/absolute/model.cgns")).isValid());
    AEROSIM_CHECK(context,
        !aerosim::ResourceLocation::projectRelative(
            QStringLiteral("C:/absolute/model.cgns")).isValid());

    AEROSIM_CHECK(context,
        aerosim::ResourceLocation::externalAbsolute(
            QStringLiteral("/var/data/model.cgns")).isValid());
    AEROSIM_CHECK(context,
        aerosim::ResourceLocation::externalAbsolute(
            QStringLiteral("C:/data/model.cgns")).isValid());
    AEROSIM_CHECK(context,
        !aerosim::ResourceLocation::externalAbsolute(
            QStringLiteral("data/model.cgns")).isValid());
    AEROSIM_CHECK(context,
        aerosim::ResourceLocation::uri(
            QStringLiteral("https://example.invalid/model.cgns")).isValid());
    AEROSIM_CHECK(context,
        !aerosim::ResourceLocation::uri(
            QStringLiteral("model.cgns")).isValid());

    const aerosim::ContentFingerprint notComputed;
    AEROSIM_CHECK(context, notComputed.isValid());
    AEROSIM_CHECK(context, !notComputed.isComputed());
    aerosim::ContentFingerprint verified;
    verified.status = aerosim::FingerprintStatus::Verified;
    verified.algorithm = QStringLiteral("sha256");
    verified.digest = QStringLiteral("0123456789abcdef");
    verified.sizeBytes = 1024;
    AEROSIM_CHECK(context, verified.isValid());
    AEROSIM_CHECK(context, verified.isComputed());
    AEROSIM_CHECK(context, verified.isVerified());
    aerosim::ContentFingerprint incomplete = verified;
    incomplete.digest.clear();
    AEROSIM_CHECK(context, !incomplete.isValid());

    aerosim::ResourceRefV2 resource;
    resource.projectId = QStringLiteral("project-a");
    resource.resourceId = QStringLiteral("resource-a");
    resource.revision = 1;
    resource.type = aerosim::ResourceType::Mesh;
    resource.location = relative;
    resource.fingerprint = verified;
    AEROSIM_CHECK(context, resource.isValid());
    AEROSIM_CHECK(context, resource.hasVerifiedContent());
    aerosim::ResourceRefV2 nextRevision = resource;
    nextRevision.revision = 2;
    AEROSIM_CHECK(context, resource.sameIdentity(nextRevision));
    AEROSIM_CHECK(context, !resource.sameVersion(nextRevision));
    aerosim::ResourceRefV2 otherProject = resource;
    otherProject.projectId = QStringLiteral("project-b");
    AEROSIM_CHECK(context, !resource.sameIdentity(otherProject));
    resource.revision = 0;
    AEROSIM_CHECK(context, !resource.isValid());

    return context.result();
}
