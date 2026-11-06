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
    checkValueType<aerosim::ResourceRef>(context);
    checkValueType<aerosim::ScopeRef>(context);
    checkValueType<aerosim::ParameterSnapshotRef>(context);
    checkValueType<aerosim::MeshRef>(context);
    checkValueType<aerosim::ResultRef>(context);
    checkValueType<aerosim::DatasetRef>(context);
    checkValueType<aerosim::SceneObjectRef>(context);

    // PR 0 freezes the observable legacy shape only. V2 validity and identity
    // helpers are deliberately deferred until their public types exist.
    aerosim::ProjectRef project;
    AEROSIM_CHECK(context, project.projectId.isEmpty());
    AEROSIM_CHECK(context, project.filePath.isEmpty());
    AEROSIM_CHECK(context, project.revision == 0);

    project.projectId = QStringLiteral("project-001");
    project.filePath = QStringLiteral("project.json");
    project.revision = 3;
    const aerosim::ProjectRef copiedProject = project;
    AEROSIM_CHECK(context, copiedProject.projectId == project.projectId);
    AEROSIM_CHECK(context, copiedProject.filePath == project.filePath);
    AEROSIM_CHECK(context, copiedProject.revision == project.revision);

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

    aerosim::ResourceRef resource;
    resource.resourceId = QStringLiteral("mesh-resource-001");
    resource.projectId = project.projectId;
    resource.kind = QStringLiteral("mesh");
    resource.filePath = QStringLiteral("mesh/original/model.cgns");
    resource.revision = 2;
    const aerosim::ResourceRef aggregateResource{
        QStringLiteral("result-resource-001"), project.projectId,
        QStringLiteral("result"), QStringLiteral("results/result.vtk"), 4};
    AEROSIM_CHECK(context,
        aggregateResource.resourceId == QStringLiteral("result-resource-001"));
    AEROSIM_CHECK(context, aggregateResource.projectId == project.projectId);
    AEROSIM_CHECK(context, aggregateResource.kind == QStringLiteral("result"));
    AEROSIM_CHECK(context,
        aggregateResource.filePath == QStringLiteral("results/result.vtk"));
    AEROSIM_CHECK(context, aggregateResource.revision == 4);

    aerosim::MeshRef mesh;
    mesh.resource = resource;
    mesh.meshVersion = QStringLiteral("mesh-v2");
    AEROSIM_CHECK(context, mesh.resource.resourceId == resource.resourceId);
    AEROSIM_CHECK(context, mesh.resource.revision == 2);

    aerosim::ResultRef result;
    result.resource = resource;
    result.taskId = QStringLiteral("task-001");
    result.meshVersion = mesh.meshVersion;
    result.caseId = scope.caseId;
    AEROSIM_CHECK(context, result.taskId == QStringLiteral("task-001"));

    aerosim::DatasetRef dataset;
    dataset.datasetId = QStringLiteral("dataset-001");
    dataset.fileId = resource.resourceId;
    aerosim::SceneObjectRef sceneObject;
    sceneObject.sceneObjectId = QStringLiteral("scene-object-001");
    sceneObject.datasetId = dataset.datasetId;
    AEROSIM_CHECK(context, sceneObject.datasetId == dataset.datasetId);

    return context.result();
}
