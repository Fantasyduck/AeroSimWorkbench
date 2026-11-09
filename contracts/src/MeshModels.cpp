#include "aerosim/contracts/MeshModels.h"

namespace aerosim {

MeshVector3 MeshVector3::make(double x, double y, double z)
{
    MeshVector3 vector;
    vector.x = x;
    vector.y = y;
    vector.z = z;
    return vector;
}

bool MeshVector3::isFinite() const
{
    return qIsFinite(x) && qIsFinite(y) && qIsFinite(z);
}

bool MeshVector3::isZero() const
{
    return x == 0.0 && y == 0.0 && z == 0.0;
}

RgbaColor RgbaColor::fromRgb(quint8 red, quint8 green, quint8 blue)
{
    return fromRgba(red, green, blue, 255);
}

RgbaColor RgbaColor::fromRgba(quint8 red, quint8 green, quint8 blue, quint8 alpha)
{
    RgbaColor color;
    color.red = red;
    color.green = green;
    color.blue = blue;
    color.alpha = alpha;
    return color;
}

bool RgbaColor::isOpaque() const
{
    return alpha == 255;
}

bool MeshDescriptor::isValid() const
{
    if (!mesh.isValid() || name.isEmpty())
        return false;
    if (role == MeshRole::Unknown || topology == MeshTopology::Unknown)
        return false;
    if (spatialDimension < 1 || spatialDimension > 3)
        return false;
    return !lengthUnit.isEmpty();
}

bool MeshDescriptor::isComputationMesh() const
{
    return role == MeshRole::Computation;
}

bool MeshTransform::isValid() const
{
    if (!input.isValid() || !output.isValid())
        return false;
    if (input.projectId != output.projectId)
        return false;
    // A transform must publish a new mesh version, never overwrite the input.
    if (output.sameVersion(input))
        return false;
    if (!(scaleFactor > 0.0) || !qIsFinite(scaleFactor))
        return false;
    if (!translation.isFinite() || !rotationDegrees.isFinite())
        return false;

    switch (kind) {
    case MeshTransformKind::Translation:
        return !translation.isZero();
    case MeshTransformKind::Rotation:
        return !rotationDegrees.isZero();
    case MeshTransformKind::Scaling:
        return scaleFactor != 1.0;
    case MeshTransformKind::UnitConversion:
        return !sourceUnit.isEmpty() && !targetUnit.isEmpty()
            && sourceUnit != targetUnit;
    case MeshTransformKind::Composite:
        return !translation.isZero() || !rotationDegrees.isZero()
            || scaleFactor != 1.0
            || (!sourceUnit.isEmpty() && !targetUnit.isEmpty()
                && sourceUnit != targetUnit);
    }
    return false;
}

bool MeshTransform::changesGeometry() const
{
    // Every MeshTransform changes computation geometry; only MeshPresentation is
    // display-only.
    return true;
}

bool MeshTransform::affectsComputationGeometry() const
{
    return changesGeometry();
}

bool MeshPresentation::isValid() const
{
    if (projectId.isEmpty() || meshId.isEmpty() || sceneId.isEmpty())
        return false;
    if (representation == MeshRepresentation::Unknown)
        return false;
    if (opacityPercent < 0 || opacityPercent > 100)
        return false;
    return displayScale > 0.0 && qIsFinite(displayScale);
}

bool MeshPresentation::affectsGeometry() const
{
    return false;
}

bool MeshPresentation::belongsTo(const MeshRefV2 &mesh) const
{
    return mesh.isValid() && projectId == mesh.projectId && meshId == mesh.meshId;
}

bool MeshCollection::isValid() const
{
    if (projectId.isEmpty() || sceneId.isEmpty())
        return false;
    if (!hasUniqueMeshIds())
        return false;
    for (const MeshDescriptor &descriptor : meshes) {
        if (!descriptor.isValid() || descriptor.mesh.projectId != projectId)
            return false;
    }
    // A running scope selects at most one computation mesh.
    if (computationMeshCount() > 1)
        return false;
    if (!computationMeshId.isEmpty() && findMesh(computationMeshId) == nullptr)
        return false;
    return true;
}

bool MeshCollection::hasUniqueMeshIds() const
{
    for (int i = 0; i < meshes.size(); ++i) {
        for (int j = i + 1; j < meshes.size(); ++j) {
            if (meshes.at(i).mesh.meshId == meshes.at(j).mesh.meshId)
                return false;
        }
    }
    return true;
}

int MeshCollection::computationMeshCount() const
{
    int count = 0;
    for (const MeshDescriptor &descriptor : meshes) {
        if (descriptor.isComputationMesh())
            ++count;
    }
    return count;
}

const MeshDescriptor *MeshCollection::findMesh(const QString &meshId) const
{
    for (const MeshDescriptor &descriptor : meshes) {
        if (descriptor.mesh.meshId == meshId)
            return &descriptor;
    }
    return nullptr;
}

const MeshDescriptor *MeshCollection::computationMesh() const
{
    if (!computationMeshId.isEmpty()) {
        const MeshDescriptor *selected = findMesh(computationMeshId);
        if (selected != nullptr)
            return selected;
    }
    for (const MeshDescriptor &descriptor : meshes) {
        if (descriptor.isComputationMesh())
            return &descriptor;
    }
    return nullptr;
}

} // namespace aerosim
