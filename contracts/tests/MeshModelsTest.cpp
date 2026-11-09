#include "TestSupport.h"

#include "aerosim/contracts/EnumTokens.h"

namespace {

aerosim::MeshDescriptor descriptor(const QString &meshId,
                                  aerosim::MeshRole role,
                                  quint64 revision = 1)
{
    aerosim::MeshDescriptor mesh;
    mesh.mesh.projectId = QStringLiteral("project-a");
    mesh.mesh.meshId = meshId;
    mesh.mesh.revision = revision;
    mesh.name = meshId;
    mesh.role = role;
    mesh.topology = aerosim::MeshTopology::Unstructured;
    mesh.spatialDimension = 3;
    mesh.coordinateSystem = QStringLiteral("cartesian");
    mesh.lengthUnit = QStringLiteral("m");
    mesh.schemaVersion = 1;
    return mesh;
}

void checkMeshReference(aerosim_test::TestContext &context)
{
    const aerosim::MeshRefV2 mesh{QStringLiteral("project-a"),
                                  QStringLiteral("mesh-a"), 2};
    AEROSIM_CHECK(context, mesh.isValid());
    aerosim::MeshRefV2 next = mesh;
    next.revision = 3;
    AEROSIM_CHECK(context, mesh.sameIdentity(next));
    AEROSIM_CHECK(context, !mesh.sameVersion(next));
    AEROSIM_CHECK(context, !aerosim::MeshRefV2().isValid());
}

void checkMeshDescriptor(aerosim_test::TestContext &context)
{
    aerosim::MeshDescriptor mesh = descriptor(QStringLiteral("mesh-a"),
                                             aerosim::MeshRole::Computation);
    AEROSIM_CHECK(context, mesh.isValid());
    AEROSIM_CHECK(context, mesh.isComputationMesh());

    mesh.role = aerosim::MeshRole::Unknown;
    AEROSIM_CHECK(context, !mesh.isValid());
    mesh.role = aerosim::MeshRole::Reference;
    AEROSIM_CHECK(context, !mesh.isComputationMesh());
    AEROSIM_CHECK(context, mesh.isValid());

    mesh.spatialDimension = 4;
    AEROSIM_CHECK(context, !mesh.isValid());
    mesh.spatialDimension = 3;
    mesh.lengthUnit.clear();
    AEROSIM_CHECK(context, !mesh.isValid());
}

void checkMeshVectorAndColor(aerosim_test::TestContext &context)
{
    const aerosim::MeshVector3 zero;
    AEROSIM_CHECK(context, zero.isFinite());
    AEROSIM_CHECK(context, zero.isZero());

    const aerosim::MeshVector3 unit =
        aerosim::MeshVector3::make(1.0, 2.0, 3.0);
    AEROSIM_CHECK(context, unit.isFinite());
    AEROSIM_CHECK(context, !unit.isZero());

    const aerosim::RgbaColor opaque =
        aerosim::RgbaColor::fromRgb(59, 130, 196);
    AEROSIM_CHECK(context, opaque.isOpaque());
    AEROSIM_CHECK(context, opaque.alpha == 255);
    AEROSIM_CHECK(context, !aerosim::RgbaColor::fromRgba(0, 0, 0, 128).isOpaque());
}

void checkMeshTransform(aerosim_test::TestContext &context)
{
    aerosim::MeshTransform translation;
    translation.kind = aerosim::MeshTransformKind::Translation;
    translation.input = {QStringLiteral("project-a"), QStringLiteral("mesh-a"), 1};
    translation.output = {QStringLiteral("project-a"), QStringLiteral("mesh-a"), 2};
    translation.translation = aerosim::MeshVector3::make(1.0, 0.0, 0.0);
    AEROSIM_CHECK(context, translation.isValid());
    AEROSIM_CHECK(context, translation.changesGeometry());
    AEROSIM_CHECK(context, translation.affectsComputationGeometry());

    // A transform must publish a new mesh version, not overwrite its input.
    translation.output = translation.input;
    AEROSIM_CHECK(context, !translation.isValid());
    translation.output = {QStringLiteral("project-a"), QStringLiteral("mesh-a"), 2};

    // A pure translation with a zero vector is not a transform.
    translation.translation = aerosim::MeshVector3();
    AEROSIM_CHECK(context, !translation.isValid());
    translation.translation = aerosim::MeshVector3::make(0.0, 0.0, 2.5);

    // A transform cannot move a mesh into another project.
    translation.output.projectId = QStringLiteral("project-b");
    AEROSIM_CHECK(context, !translation.isValid());
    translation.output.projectId = QStringLiteral("project-a");

    aerosim::MeshTransform conversion;
    conversion.kind = aerosim::MeshTransformKind::UnitConversion;
    conversion.input = {QStringLiteral("project-a"), QStringLiteral("mesh-a"), 1};
    conversion.output = {QStringLiteral("project-a"), QStringLiteral("mesh-mm"), 1};
    conversion.sourceUnit = QStringLiteral("m");
    conversion.targetUnit = QStringLiteral("mm");
    AEROSIM_CHECK(context, conversion.isValid());
    conversion.targetUnit = QStringLiteral("m");
    AEROSIM_CHECK(context, !conversion.isValid());

    // A non-positive scale factor is never valid.
    aerosim::MeshTransform scaling;
    scaling.kind = aerosim::MeshTransformKind::Scaling;
    scaling.input = {QStringLiteral("project-a"), QStringLiteral("mesh-a"), 1};
    scaling.output = {QStringLiteral("project-a"), QStringLiteral("mesh-a"), 2};
    scaling.scaleFactor = 0.0;
    AEROSIM_CHECK(context, !scaling.isValid());
    scaling.scaleFactor = 2.0;
    AEROSIM_CHECK(context, scaling.isValid());
}

void checkMeshPresentation(aerosim_test::TestContext &context)
{
    const aerosim::MeshRefV2 mesh{QStringLiteral("project-a"),
                                  QStringLiteral("mesh-a"), 2};
    aerosim::MeshPresentation presentation;
    AEROSIM_CHECK(context, !presentation.isValid());

    presentation.projectId = QStringLiteral("project-a");
    presentation.meshId = QStringLiteral("mesh-a");
    presentation.sceneId = QStringLiteral("scene-main");
    presentation.representation = aerosim::MeshRepresentation::Wireframe;
    presentation.color = aerosim::RgbaColor::fromRgb(20, 120, 200);
    presentation.opacityPercent = 65;
    presentation.displayScale = 1000.0;      // display ratio only
    presentation.displayUnit = QStringLiteral("mm");
    AEROSIM_CHECK(context, presentation.isValid());
    AEROSIM_CHECK(context, presentation.belongsTo(mesh));
    // A display preference never changes geometry or mesh identity.
    AEROSIM_CHECK(context, !presentation.affectsGeometry());

    presentation.opacityPercent = 101;
    AEROSIM_CHECK(context, !presentation.isValid());
    presentation.opacityPercent = 100;
    presentation.displayScale = 0.0;
    AEROSIM_CHECK(context, !presentation.isValid());
    presentation.displayScale = 1.0;
    presentation.representation = aerosim::MeshRepresentation::Unknown;
    AEROSIM_CHECK(context, !presentation.isValid());

    const aerosim::MeshRefV2 otherMesh{QStringLiteral("project-a"),
                                       QStringLiteral("mesh-b"), 1};
    AEROSIM_CHECK(context, !presentation.belongsTo(otherMesh));
}

void checkMeshCollection(aerosim_test::TestContext &context)
{
    aerosim::MeshCollection collection;
    AEROSIM_CHECK(context, !collection.isValid());

    collection.projectId = QStringLiteral("project-a");
    collection.sceneId = QStringLiteral("scene-main");
    collection.meshes.append(descriptor(QStringLiteral("mesh-a"),
                                        aerosim::MeshRole::Computation));
    collection.meshes.append(descriptor(QStringLiteral("mesh-b"),
                                        aerosim::MeshRole::Reference));
    collection.meshes.append(descriptor(QStringLiteral("mesh-c"),
                                        aerosim::MeshRole::Result));
    AEROSIM_CHECK(context, collection.isValid());
    AEROSIM_CHECK(context, collection.hasUniqueMeshIds());
    AEROSIM_CHECK(context, collection.meshes.size() == 3);
    AEROSIM_CHECK(context, collection.computationMeshCount() == 1);
    AEROSIM_CHECK(context, collection.findMesh(QStringLiteral("mesh-b")) != nullptr);
    AEROSIM_CHECK(context, collection.findMesh(QStringLiteral("missing")) == nullptr);
    AEROSIM_CHECK(context, collection.computationMesh() != nullptr);
    AEROSIM_CHECK(context,
        collection.computationMesh()->mesh.meshId == QStringLiteral("mesh-a"));
    // Reference and result meshes never become solver input automatically.
    AEROSIM_CHECK(context,
        !collection.findMesh(QStringLiteral("mesh-b"))->isComputationMesh());

    // Selecting an explicit computation mesh that is not present is invalid.
    collection.computationMeshId = QStringLiteral("mesh-z");
    AEROSIM_CHECK(context, !collection.isValid());
    collection.computationMeshId = QStringLiteral("mesh-c");
    AEROSIM_CHECK(context, collection.isValid());
    AEROSIM_CHECK(context,
        collection.computationMesh()->mesh.meshId == QStringLiteral("mesh-c"));
    collection.computationMeshId.clear();

    // At most one computation mesh per scope.
    aerosim::MeshDescriptor secondComputation =
        descriptor(QStringLiteral("mesh-d"), aerosim::MeshRole::Computation);
    collection.meshes.append(secondComputation);
    AEROSIM_CHECK(context, collection.computationMeshCount() == 2);
    AEROSIM_CHECK(context, !collection.isValid());
    collection.meshes.removeLast();

    // Duplicate mesh identities are rejected.
    collection.meshes.append(descriptor(QStringLiteral("mesh-b"),
                                        aerosim::MeshRole::Auxiliary));
    AEROSIM_CHECK(context, !collection.hasUniqueMeshIds());
    AEROSIM_CHECK(context, !collection.isValid());
    collection.meshes.removeLast();

    // A mesh from another project must not appear in this collection.
    aerosim::MeshDescriptor foreign = descriptor(QStringLiteral("mesh-x"),
                                                aerosim::MeshRole::Auxiliary);
    foreign.mesh.projectId = QStringLiteral("project-b");
    collection.meshes.append(foreign);
    AEROSIM_CHECK(context, !collection.isValid());
    collection.meshes.removeLast();
    AEROSIM_CHECK(context, collection.isValid());
}

void checkDisplayLimit(aerosim_test::TestContext &context)
{
    // "At most 10 overlays" is a UI/product display constraint only.
    AEROSIM_CHECK(context, aerosim::kSceneMeshDisplayLimit == 10);
    AEROSIM_CHECK(context, aerosim::isWithinMeshDisplayLimit(0));
    AEROSIM_CHECK(context, aerosim::isWithinMeshDisplayLimit(10));
    AEROSIM_CHECK(context, !aerosim::isWithinMeshDisplayLimit(11));

    // The container itself is not capped at the display limit.
    aerosim::MeshCollection collection;
    collection.projectId = QStringLiteral("project-a");
    collection.sceneId = QStringLiteral("scene-main");
    for (int index = 0; index < 12; ++index) {
        collection.meshes.append(descriptor(QStringLiteral("mesh-%1").arg(index),
            index == 0 ? aerosim::MeshRole::Computation
                       : aerosim::MeshRole::Reference));
    }
    AEROSIM_CHECK(context, collection.meshes.size() == 12);
    AEROSIM_CHECK(context, collection.isValid());
}

void checkTokens(aerosim_test::TestContext &context)
{
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::MeshRole::Computation)
            == QStringLiteral("computation"));
    AEROSIM_CHECK(context,
        aerosim::meshRoleFromToken(QStringLiteral("auxiliary")).value()
            == aerosim::MeshRole::Auxiliary);
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::MeshTopology::PointCloud)
            == QStringLiteral("point_cloud"));
    AEROSIM_CHECK(context,
        aerosim::meshTopologyFromToken(QStringLiteral("hybrid")).value()
            == aerosim::MeshTopology::Hybrid);
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::MeshRepresentation::SurfaceWithEdges)
            == QStringLiteral("surface_with_edges"));
    AEROSIM_CHECK(context,
        aerosim::meshRepresentationFromToken(QStringLiteral("points")).value()
            == aerosim::MeshRepresentation::Points);
    AEROSIM_CHECK(context,
        aerosim::enumToken(aerosim::MeshTransformKind::UnitConversion)
            == QStringLiteral("unit_conversion"));
    AEROSIM_CHECK(context,
        aerosim::meshTransformKindFromToken(QStringLiteral("composite")).value()
            == aerosim::MeshTransformKind::Composite);
    AEROSIM_CHECK(context,
        aerosim::meshRoleFromToken(QStringLiteral("nope")).status()
            == aerosim::AccessStatus::InvalidArgument);
}

} // namespace

int main()
{
    aerosim_test::TestContext context;
    checkMeshReference(context);
    checkMeshDescriptor(context);
    checkMeshVectorAndColor(context);
    checkMeshTransform(context);
    checkMeshPresentation(context);
    checkMeshCollection(context);
    checkDisplayLimit(context);
    checkTokens(context);
    return context.result();
}
