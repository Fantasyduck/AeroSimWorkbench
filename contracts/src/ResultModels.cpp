#include "aerosim/contracts/ResultModels.h"

namespace aerosim {

bool FieldDescriptor::isValid() const
{
    if (key.isEmpty() || componentCount < 1)
        return false;
    if (isVector && componentCount != 3)
        return false;
    return association != DatasetFieldAssociation::Unknown;
}

bool FieldDescriptor::isScalar() const
{
    return componentCount == 1 && !isVector;
}

bool DatasetZoneDescriptor::isValid() const
{
    if (zoneId.isEmpty())
        return false;
    for (const FieldDescriptor &field : fields) {
        if (!field.isValid())
            return false;
    }
    return true;
}

bool ResultDescriptor::isValid() const
{
    if (projectId.isEmpty() || resultId.isEmpty() || revision == 0)
        return false;
    if (!task.isValid() || !attempt.isValid())
        return false;
    if (!attempt.task.sameIdentity(task))
        return false;
    if (!resource.isValid() || resource.type != ResourceType::Result)
        return false;
    if (resource.projectId != projectId || task.projectId != projectId)
        return false;
    return kind != ResultKind::Unknown;
}

bool ResultDescriptor::sameIdentity(const ResultDescriptor &other) const
{
    return projectId == other.projectId && resultId == other.resultId;
}

bool ResultDescriptor::sameVersion(const ResultDescriptor &other) const
{
    return sameIdentity(other) && revision == other.revision;
}

bool DatasetDescriptor::isValid() const
{
    if (ref.datasetId.isEmpty())
        return false;
    if (!resultResource.isValid() || resultResource.type != ResourceType::Result)
        return false;
    if (ref.fileId.isEmpty())
        return false;
    for (const FieldDescriptor &field : fields) {
        if (!field.isValid())
            return false;
    }
    for (const DatasetZoneDescriptor &zone : zones) {
        if (!zone.isValid())
            return false;
    }
    return hasUniqueZoneIds();
}

bool DatasetDescriptor::hasUniqueZoneIds() const
{
    for (int i = 0; i < zones.size(); ++i) {
        for (int j = i + 1; j < zones.size(); ++j) {
            if (zones.at(i).zoneId == zones.at(j).zoneId)
                return false;
        }
    }
    return true;
}

const FieldDescriptor *DatasetDescriptor::findField(const QString &key) const
{
    for (const FieldDescriptor &field : fields) {
        if (field.key == key)
            return &field;
    }
    return nullptr;
}

const DatasetZoneDescriptor *
DatasetDescriptor::findZone(const QString &zoneId) const
{
    for (const DatasetZoneDescriptor &zone : zones) {
        if (zone.zoneId == zoneId)
            return &zone;
    }
    return nullptr;
}

bool SceneObjectStyle::isValid() const
{
    if (opacityPercent < 0 || opacityPercent > 100)
        return false;
    if (!(lineWidth > 0.0) || !qIsFinite(lineWidth))
        return false;
    if (showColorMap && representation == MeshRepresentation::Unknown)
        return false;
    return true;
}

bool SceneObjectDescriptor::isValid() const
{
    if (ref.sceneObjectId.isEmpty() || ref.datasetId.isEmpty())
        return false;
    if (dataset.datasetId.isEmpty() || dataset.datasetId != ref.datasetId)
        return false;
    if (kind == SceneObjectKind::Unknown)
        return false;
    if (!style.isValid())
        return false;
    if (requiresSourceMesh() && !sourceMesh.isValid())
        return false;
    return true;
}

bool SceneObjectDescriptor::requiresSourceMesh() const
{
    switch (kind) {
    case SceneObjectKind::SourceMesh:
    case SceneObjectKind::Slice:
    case SceneObjectKind::IsoSurface:
    case SceneObjectKind::Contour:
    case SceneObjectKind::Streamline:
    case SceneObjectKind::Pathline:
    case SceneObjectKind::Surface:
        return true;
    case SceneObjectKind::Curve:
    case SceneObjectKind::Annotation:
    case SceneObjectKind::Unknown:
        return false;
    }
    return false;
}

bool PostProcessScene::isValid() const
{
    if (projectId.isEmpty() || sceneId.isEmpty())
        return false;
    if (!hasUniqueObjectIds())
        return false;
    for (const SceneObjectDescriptor &object : objects) {
        if (!object.isValid())
            return false;
    }
    return true;
}

bool PostProcessScene::hasUniqueObjectIds() const
{
    for (int i = 0; i < objects.size(); ++i) {
        for (int j = i + 1; j < objects.size(); ++j) {
            if (objects.at(i).ref.sceneObjectId == objects.at(j).ref.sceneObjectId)
                return false;
        }
    }
    return true;
}

const SceneObjectDescriptor *
PostProcessScene::findObject(const QString &sceneObjectId) const
{
    for (const SceneObjectDescriptor &object : objects) {
        if (object.ref.sceneObjectId == sceneObjectId)
            return &object;
    }
    return nullptr;
}

} // namespace aerosim
