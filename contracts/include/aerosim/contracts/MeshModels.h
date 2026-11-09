#pragma once

#include "aerosim/contracts/References.h"

#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace aerosim {

// MeshRefV2 identifies a mesh aggregate and one of its revisions. It carries no
// geometry: node/connectivity arrays, readers and VTK objects stay in the data
// module and are never copied into contracts.
struct MeshRefV2 {
    QString projectId;
    QString meshId;
    quint64 revision = 0;

    bool isValid() const
    {
        return !projectId.isEmpty() && !meshId.isEmpty() && revision > 0;
    }
    bool sameIdentity(const MeshRefV2 &other) const
    {
        return projectId == other.projectId && meshId == other.meshId;
    }
    bool sameVersion(const MeshRefV2 &other) const
    {
        return sameIdentity(other) && revision == other.revision;
    }
};

// Computation is the single mesh a scope uses as solver input. Reference,
// Result and Auxiliary meshes may be displayed next to it but never become
// solver input automatically.
enum class MeshRole { Unknown, Computation, Reference, Result, Auxiliary };

enum class MeshTopology { Unknown, Structured, Unstructured, Hybrid, PointCloud };

// Display-only appearance. Kept in Core contracts as a plain RGBA value so the
// public headers never depend on Qt Widgets or QColor.
enum class MeshRepresentation { Unknown, Surface, SurfaceWithEdges, Wireframe, Points };

enum class MeshTransformKind { Translation, Rotation, Scaling, UnitConversion, Composite };

struct MeshVector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    bool isFinite() const;
    bool isZero() const;

    static MeshVector3 make(double x, double y, double z);
};

struct RgbaColor {
    quint8 red = 255;
    quint8 green = 255;
    quint8 blue = 255;
    quint8 alpha = 255;

    static RgbaColor fromRgb(quint8 red, quint8 green, quint8 blue);
    static RgbaColor fromRgba(quint8 red, quint8 green, quint8 blue, quint8 alpha);
    bool isOpaque() const;
};

// Describes a mesh aggregate without exposing its arrays. A descriptor names the
// resource, topology, dimension, coordinate system and unit so results and tasks
// can reference a determined mesh version.
struct MeshDescriptor {
    MeshRefV2 mesh;
    QString name;
    MeshRole role = MeshRole::Unknown;
    MeshTopology topology = MeshTopology::Unknown;
    int spatialDimension = 0;
    QString coordinateSystem;
    QString lengthUnit;
    ResourceRefV2 source;      // optional provenance resource
    quint64 schemaVersion = 0;

    bool isValid() const;
    bool isComputationMesh() const;
};

// A geometry-changing transform. Applying it produces a new processed MeshRefV2;
// display-only scaling belongs to MeshPresentation instead. A failed transform
// must not publish a half-written mesh.
struct MeshTransform {
    MeshTransformKind kind = MeshTransformKind::Translation;
    MeshVector3 translation;
    MeshVector3 rotationDegrees;
    double scaleFactor = 1.0;
    QString sourceUnit;
    QString targetUnit;
    MeshRefV2 input;
    MeshRefV2 output;

    bool isValid() const;
    bool changesGeometry() const;
    bool affectsComputationGeometry() const;
};

// Per-mesh display preference stored with the project scene, not with global
// window layout settings. It never changes mesh coordinates, revision or
// fingerprint.
struct MeshPresentation {
    QString projectId;
    QString meshId;
    QString sceneId;
    bool visible = true;
    MeshRepresentation representation = MeshRepresentation::Surface;
    RgbaColor color;
    int opacityPercent = 100;
    double displayScale = 1.0;
    QString displayUnit;

    bool isValid() const;
    bool affectsGeometry() const;
    bool belongsTo(const MeshRefV2 &mesh) const;
};

// A workspace may hold many meshes. Containers are deliberately not capped at
// the UI display limit.
struct MeshCollection {
    QString projectId;
    QString sceneId;
    QVector<MeshDescriptor> meshes;
    QString computationMeshId;

    bool isValid() const;
    bool hasUniqueMeshIds() const;
    int computationMeshCount() const;
    const MeshDescriptor *findMesh(const QString &meshId) const;
    const MeshDescriptor *computationMesh() const;
};

// The mesh page shows at most this many overlays at once. It is a UI/product
// display constraint only: MeshCollection and other containers must keep
// accepting more elements.
constexpr int kSceneMeshDisplayLimit = 10;
inline bool isWithinMeshDisplayLimit(int displayedMeshCount)
{
    return displayedMeshCount <= kSceneMeshDisplayLimit;
}

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::MeshRefV2)
Q_DECLARE_METATYPE(aerosim::MeshVector3)
Q_DECLARE_METATYPE(aerosim::RgbaColor)
Q_DECLARE_METATYPE(aerosim::MeshDescriptor)
Q_DECLARE_METATYPE(aerosim::MeshTransform)
Q_DECLARE_METATYPE(aerosim::MeshPresentation)
Q_DECLARE_METATYPE(aerosim::MeshCollection)

