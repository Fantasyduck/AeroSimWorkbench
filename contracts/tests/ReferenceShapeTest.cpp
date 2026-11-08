#include "TestSupport.h"

#include "aerosim/contracts/PostProcessRefs.h"
#include "aerosim/contracts/References.h"

#include <type_traits>

namespace {

template <typename T>
void checkValueType(aerosim_test::TestContext &context)
{
    AEROSIM_CHECK(context, std::is_default_constructible<T>::value);
    AEROSIM_CHECK(context, std::is_copy_constructible<T>::value);
    AEROSIM_CHECK(context, std::is_copy_assignable<T>::value);
}

} // namespace

int main()
{
    aerosim_test::TestContext context;

    checkValueType<aerosim::ProjectRef>(context);
    checkValueType<aerosim::ScopeRef>(context);
    checkValueType<aerosim::ParameterSnapshotRef>(context);
    checkValueType<aerosim::ResourceRefV2>(context);
    checkValueType<aerosim::MeshRef>(context);
    checkValueType<aerosim::ResultRef>(context);
    checkValueType<aerosim::DatasetRef>(context);
    checkValueType<aerosim::SceneObjectRef>(context);

    // References.h now exposes the unified typed ResourceRefV2. The duplicate
    // legacy kind/filePath ResourceRef shape has been removed, so MeshRef and
    // ResultRef carry the same resource identity as every other caller.
    aerosim::ProjectRef project;
    AEROSIM_CHECK(context, project.projectId.isEmpty());
    AEROSIM_CHECK(context, project.filePath.isEmpty());
    AEROSIM_CHECK(context, project.revision == 0);

    project.projectId = QStringLiteral("project-001");
    project.filePath = QStringLiteral("project.json");
    project.revision = 3;

    aerosim::ScopeRef scope;
    scope.projectId = project.projectId;
    scope.caseId = QStringLiteral("State_001");
    scope.objectId = QStringLiteral("mesh-main");
    AEROSIM_CHECK(context, scope.projectId == QStringLiteral("project-001"));
    AEROSIM_CHECK(context, scope.caseId == QStringLiteral("State_001"));

    aerosim::ParameterSnapshotRef snapshot;
    AEROSIM_CHECK(context, snapshot.snapshotId.isEmpty());
    AEROSIM_CHECK(context, snapshot.revision == 0);
    snapshot.snapshotId = QStringLiteral("snapshot-001");
    snapshot.scope = scope;
    snapshot.revision = 7;
    AEROSIM_CHECK(context, snapshot.scope.caseId == scope.caseId);

    aerosim::ResourceRefV2 resource;
    resource.projectId = project.projectId;
    resource.resourceId = QStringLiteral("mesh-resource-001");
    resource.revision = 2;
    resource.type = aerosim::ResourceType::Mesh;
    resource.location = aerosim::ResourceLocation::projectRelative(
        QStringLiteral("mesh/original/model.cgns"));
    AEROSIM_CHECK(context, resource.isValid());

    aerosim::MeshRef mesh;
    mesh.resource = resource;
    mesh.meshVersion = QStringLiteral("mesh-v2");
    AEROSIM_CHECK(context, mesh.resource.resourceId == resource.resourceId);
    AEROSIM_CHECK(context, mesh.resource.revision == 2);
    AEROSIM_CHECK(context, mesh.resource.sameVersion(resource));

    aerosim::ResultRef result;
    result.resource = resource;
    result.taskId = QStringLiteral("task-001");
    result.meshVersion = mesh.meshVersion;
    result.caseId = scope.caseId;
    AEROSIM_CHECK(context, result.taskId == QStringLiteral("task-001"));
    AEROSIM_CHECK(context, result.resource.sameIdentity(resource));

    aerosim::DatasetRef dataset;
    dataset.datasetId = QStringLiteral("dataset-001");
    dataset.fileId = resource.resourceId;
    aerosim::SceneObjectRef sceneObject;
    sceneObject.sceneObjectId = QStringLiteral("scene-object-001");
    sceneObject.datasetId = dataset.datasetId;
    AEROSIM_CHECK(context, sceneObject.datasetId == dataset.datasetId);

    return context.result();
}
