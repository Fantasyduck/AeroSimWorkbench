#include "TestSupport.h"

#include "aerosim/contracts/EnumTokens.h"

namespace {

aerosim::ResourceRefV2 resultResource(const QString &resourceId)
{
    aerosim::ResourceRefV2 resource;
    resource.projectId = QStringLiteral("project-a");
    resource.resourceId = resourceId;
    resource.revision = 1;
    resource.type = aerosim::ResourceType::Result;
    resource.location = aerosim::ResourceLocation::projectRelative(
        QStringLiteral("results/flowfield.cgns"));
    resource.fingerprint.status = aerosim::FingerprintStatus::Verified;
    resource.fingerprint.algorithm = QStringLiteral("sha256");
    resource.fingerprint.digest = QStringLiteral("beefcafe");
    return resource;
}

aerosim::MeshRefV2 meshRef()
{
    aerosim::MeshRefV2 mesh;
    mesh.projectId = QStringLiteral("project-a");
    mesh.meshId = QStringLiteral("mesh-a");
    mesh.revision = 3;
    return mesh;
}

aerosim::FieldDescriptor scalarField(const QString &key, const QString &unit)
{
    aerosim::FieldDescriptor field;
    field.key = key;
    field.displayName = key;
    field.unit = unit;
    field.componentCount = 1;
    field.isVector = false;
    field.association = aerosim::DatasetFieldAssociation::Point;
    return field;
}

aerosim::FieldDescriptor vectorField(const QString &key, const QString &unit)
{
    aerosim::FieldDescriptor field;
    field.key = key;
    field.displayName = key;
    field.unit = unit;
    field.componentCount = 3;
    field.isVector = true;
    field.association = aerosim::DatasetFieldAssociation::Point;
    return field;
}

aerosim::ResultDescriptor resultOf(const QString &resultId,
                                  const aerosim::AttemptRef &attempt)
{
    aerosim::ResultDescriptor result;
    result.projectId = QStringLiteral("project-a");
    result.resultId = resultId;
    result.revision = 1;
    result.task = attempt.task;
    result.attempt = attempt;
    result.resource = resultResource(QStringLiteral("resource-%1").arg(resultId));
    result.kind = aerosim::ResultKind::Field;
    return result;
}

void checkFields(aerosim_test::TestContext &context)
{
    aerosim::FieldDescriptor field;
    AEROSIM_CHECK(context, !field.isValid());
    field.key = QStringLiteral("mach");
    AEROSIM_CHECK(context, !field.isValid());   // association still unknown
    field.association = aerosim::DatasetFieldAssociation::Point;
    field.displayName = QStringLiteral("Mach");
    field.unit = QStringLiteral("1");
    AEROSIM_CHECK(context, field.isValid());
    AEROSIM_CHECK(context, field.isScalar());

    const aerosim::FieldDescriptor velocity = vectorField(QStringLiteral("velocity"),
                                                         QStringLiteral("m/s"));
    AEROSIM_CHECK(context, velocity.isValid());
    AEROSIM_CHECK(context, !velocity.isScalar());

    // A vector field declares three components, never fewer.
    aerosim::FieldDescriptor broken = velocity;
    broken.componentCount = 1;
    AEROSIM_CHECK(context, !broken.isValid());

    // A cell-centred field is distinct from a point field.
    aerosim::FieldDescriptor cellField = scalarField(QStringLiteral("pressure"),
                                                     QStringLiteral("Pa"));
    cellField.association = aerosim::DatasetFieldAssociation::Cell;
    AEROSIM_CHECK(context, cellField.isValid());
    AEROSIM_CHECK(context,
        cellField.association != velocity.association);
}

void checkResultDescriptor(aerosim_test::TestContext &context)
{
    aerosim::AttemptRef attempt;
    attempt.task.projectId = QStringLiteral("project-a");
    attempt.task.taskId = QStringLiteral("task-a");
    attempt.attemptId = QStringLiteral("attempt-1");

    aerosim::ResultDescriptor result;
    AEROSIM_CHECK(context, !result.isValid());

    result = resultOf(QStringLiteral("result-1"), attempt);
    AEROSIM_CHECK(context, result.isValid());
    AEROSIM_CHECK(context, result.sameVersion(resultOf(QStringLiteral("result-1"),
                                                      attempt)));

    aerosim::ResultDescriptor next = result;
    next.revision = 2;
    AEROSIM_CHECK(context, result.sameIdentity(next));
    AEROSIM_CHECK(context, !result.sameVersion(next));

    // A result resource must be typed as a result.
    result.resource.type = aerosim::ResourceType::Log;
    AEROSIM_CHECK(context, !result.isValid());
    result.resource.type = aerosim::ResourceType::Result;

    // The attempt must belong to the owning task.
    result.attempt.task.taskId = QStringLiteral("task-b");
    AEROSIM_CHECK(context, !result.isValid());
    result.attempt = attempt;

    // One task can produce several results.
    const aerosim::ResultDescriptor surface = resultOf(QStringLiteral("result-2"),
                                                       attempt);
    AEROSIM_CHECK(context, surface.isValid());
    AEROSIM_CHECK(context, surface.task.sameIdentity(result.task));
    AEROSIM_CHECK(context, !surface.sameIdentity(result));
}

void checkDatasetDescriptor(aerosim_test::TestContext &context)
{
    aerosim::DatasetDescriptor dataset;
    AEROSIM_CHECK(context, !dataset.isValid());

    dataset.ref.datasetId = QStringLiteral("dataset-1");
    dataset.ref.fileId = QStringLiteral("resource-result-1");
    dataset.name = QStringLiteral("Ma5 flowfield");
    dataset.kind = aerosim::ResultKind::Field;
    dataset.resultResource = resultResource(QStringLiteral("resource-result-1"));
    dataset.mesh = meshRef();
    dataset.revision = 1;
    dataset.schemaVersion = 1;
    dataset.fields.append(scalarField(QStringLiteral("mach"), QStringLiteral("1")));
    dataset.fields.append(vectorField(QStringLiteral("velocity"),
                                      QStringLiteral("m/s")));

    aerosim::DatasetZoneDescriptor volume;
    volume.zoneId = QStringLiteral("zone-volume");
    volume.name = QStringLiteral("Zone");
    volume.fields.append(scalarField(QStringLiteral("mach"), QStringLiteral("1")));
    dataset.zones.append(volume);

    aerosim::DatasetZoneDescriptor wing = volume;
    wing.zoneId = QStringLiteral("zone-wing");
    wing.boundary = true;
    dataset.zones.append(wing);

    AEROSIM_CHECK(context, dataset.isValid());
    AEROSIM_CHECK(context, dataset.hasUniqueZoneIds());
    AEROSIM_CHECK(context, dataset.findField(QStringLiteral("velocity")) != nullptr);
    AEROSIM_CHECK(context, dataset.findField(QStringLiteral("missing")) == nullptr);
    AEROSIM_CHECK(context, dataset.findZone(QStringLiteral("zone-wing")) != nullptr);
    AEROSIM_CHECK(context, dataset.findZone(QStringLiteral("missing")) == nullptr);
    AEROSIM_CHECK(context, dataset.findZone(QStringLiteral("zone-wing"))->boundary);
    // The dataset references the mesh version instead of copying geometry.
    AEROSIM_CHECK(context, dataset.mesh.sameVersion(meshRef()));

    // The dataset depends on a result resource, not on any other type.
    dataset.resultResource.type = aerosim::ResourceType::Mesh;
    AEROSIM_CHECK(context, !dataset.isValid());
    dataset.resultResource.type = aerosim::ResourceType::Result;

    // Duplicate zone ids are rejected.
    dataset.zones.append(volume);
    AEROSIM_CHECK(context, !dataset.hasUniqueZoneIds());
    AEROSIM_CHECK(context, !dataset.isValid());
    dataset.zones.removeLast();

    // An invalid field makes the dataset invalid.
    aerosim::FieldDescriptor invalidField;
    invalidField.key = QStringLiteral("broken");
    dataset.fields.append(invalidField);
    AEROSIM_CHECK(context, !dataset.isValid());
    dataset.fields.removeLast();
    AEROSIM_CHECK(context, dataset.isValid());
}

void checkSceneAndStyle(aerosim_test::TestContext &context)
{
    aerosim::SceneObjectStyle style;
    AEROSIM_CHECK(context, style.isValid());
    AEROSIM_CHECK(context, style.visible);
    AEROSIM_CHECK(context, style.opacityPercent == 100);

    style.opacityPercent = 140;
    AEROSIM_CHECK(context, !style.isValid());
    style.opacityPercent = 70;
    style.lineWidth = 0.0;
    AEROSIM_CHECK(context, !style.isValid());
    style.lineWidth = 1.5;

    aerosim::SceneObjectDescriptor object;
    AEROSIM_CHECK(context, !object.isValid());

    object.ref.sceneObjectId = QStringLiteral("object-1");
    object.ref.datasetId = QStringLiteral("dataset-1");
    object.dataset.datasetId = QStringLiteral("dataset-1");
    object.name = QStringLiteral("Wing surface");
    object.kind = aerosim::SceneObjectKind::Surface;
    object.style = style;
    object.styleRevision = 1;
    // A spatial feature references a determined mesh version.
    AEROSIM_CHECK(context, object.requiresSourceMesh());
    AEROSIM_CHECK(context, !object.isValid());
    object.sourceMesh = meshRef();
    AEROSIM_CHECK(context, object.isValid());

    // The scene object id must agree with the referenced dataset id.
    object.ref.datasetId = QStringLiteral("dataset-2");
    AEROSIM_CHECK(context, !object.isValid());
    object.ref.datasetId = QStringLiteral("dataset-1");

    // A curve is not a spatial feature and needs no source mesh.
    aerosim::SceneObjectDescriptor curve = object;
    curve.kind = aerosim::SceneObjectKind::Curve;
    curve.sourceMesh = aerosim::MeshRefV2();
    AEROSIM_CHECK(context, !curve.requiresSourceMesh());
    AEROSIM_CHECK(context, curve.isValid());

    // Display attributes stay separate from the raw result data.
    curve.style.visible = false;
    AEROSIM_CHECK(context, curve.isValid());
    AEROSIM_CHECK(context, !curve.style.visible);
    AEROSIM_CHECK(context, object.style.visible);
    // Style editing advances the style revision, not the dataset identity.
    curve.styleRevision = 2;
    AEROSIM_CHECK(context, curve.styleRevision != object.styleRevision);
    AEROSIM_CHECK(context, curve.dataset.datasetId == object.dataset.datasetId);
}

void checkPostProcessScene(aerosim_test::TestContext &context)
{
    aerosim::PostProcessScene scene;
    AEROSIM_CHECK(context, !scene.isValid());
    scene.projectId = QStringLiteral("project-a");
    scene.sceneId = QStringLiteral("scene-main");
    scene.name = QStringLiteral("Main scene");
    scene.revision = 1;
    scene.schemaVersion = 1;
    // An empty scene is a valid document.
    AEROSIM_CHECK(context, scene.isValid());

    aerosim::SceneObjectDescriptor object;
    object.ref.sceneObjectId = QStringLiteral("object-1");
    object.ref.datasetId = QStringLiteral("dataset-1");
    object.dataset.datasetId = QStringLiteral("dataset-1");
    object.name = QStringLiteral("Wing slice");
    object.kind = aerosim::SceneObjectKind::Slice;
    object.sourceMesh = meshRef();
    object.styleRevision = 1;
    scene.objects.append(object);
    AEROSIM_CHECK(context, scene.isValid());
    AEROSIM_CHECK(context, scene.findObject(QStringLiteral("object-1")) != nullptr);
    AEROSIM_CHECK(context, scene.findObject(QStringLiteral("missing")) == nullptr);
    AEROSIM_CHECK(context,
        scene.findObject(QStringLiteral("object-1"))->dataset.datasetId
            == QStringLiteral("dataset-1"));

    // Duplicate scene object ids are rejected.
    scene.objects.append(object);
    AEROSIM_CHECK(context, !scene.hasUniqueObjectIds());
    AEROSIM_CHECK(context, !scene.isValid());
    scene.objects.removeLast();
    AEROSIM_CHECK(context, scene.isValid());
}

void checkTokens(aerosim_test::TestContext &context)
{
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::ResultKind::Boundary) == QStringLiteral("boundary"));
    AEROSIM_CHECK(context,
        aerosim::resultKindFromToken(QStringLiteral("report")).value()
            == aerosim::ResultKind::Report);
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::DatasetFieldAssociation::Cell)
            == QStringLiteral("cell"));
    AEROSIM_CHECK(context,
        aerosim::datasetFieldAssociationFromToken(QStringLiteral("point")).value()
            == aerosim::DatasetFieldAssociation::Point);
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::SceneObjectKind::IsoSurface)
            == QStringLiteral("iso_surface"));
    AEROSIM_CHECK(context,
        aerosim::sceneObjectKindFromToken(QStringLiteral("streamline")).value()
            == aerosim::SceneObjectKind::Streamline);
    AEROSIM_CHECK(context,
        aerosim::resultKindFromToken(QStringLiteral("nope")).status()
            == aerosim::AccessStatus::InvalidArgument);
}

} // namespace

int main()
{
    aerosim_test::TestContext context;
    checkFields(context);
    checkResultDescriptor(context);
    checkDatasetDescriptor(context);
    checkSceneAndStyle(context);
    checkPostProcessScene(context);
    checkTokens(context);
    return context.result();
}
