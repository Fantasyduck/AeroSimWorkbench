#pragma once

#include "aerosim/contracts/MeshModels.h"
#include "aerosim/contracts/PostProcessRefs.h"
#include "aerosim/contracts/References.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace aerosim {

enum class ResultKind { Unknown, Field, Boundary, Curve, Report, Export };

enum class DatasetFieldAssociation { Unknown, Point, Cell, Boundary, Global };

// Metadata of one physical variable. Units and point/cell association are
// explicit; unknown units are left empty instead of being guessed.
struct FieldDescriptor {
    QString key;
    QString displayName;
    QString physicalQuantity;
    QString unit;
    int componentCount = 1;
    bool isVector = false;
    DatasetFieldAssociation association = DatasetFieldAssociation::Unknown;
    quint64 revision = 0;

    bool isValid() const;
    bool isScalar() const;
};

// A zone/boundary inside a dataset, with the fields actually available there.
struct DatasetZoneDescriptor {
    QString zoneId;
    QString name;
    bool boundary = false;
    QVector<FieldDescriptor> fields;

    bool isValid() const;
};

// One produced result file of a task attempt. A task may produce several.
struct ResultDescriptor {
    QString projectId;
    QString resultId;
    quint64 revision = 0;
    TaskRef task;
    AttemptRef attempt;
    ResourceRefV2 resource;      // must be typed ResourceType::Result
    ResultKind kind = ResultKind::Unknown;
    QDateTime createdAt;

    bool isValid() const;
    bool sameIdentity(const ResultDescriptor &other) const;
    bool sameVersion(const ResultDescriptor &other) const;
};

// One loaded dataset instance. DatasetRef stays identity-only; version and the
// spatial association live here so nothing large is copied into contracts.
struct DatasetDescriptor {
    DatasetRef ref;
    QString name;
    ResultKind kind = ResultKind::Unknown;
    ResourceRefV2 resultResource;
    MeshRefV2 mesh;             // spatial association; may be unspecified
    quint64 revision = 0;
    quint64 schemaVersion = 0;
    QVector<FieldDescriptor> fields;
    QVector<DatasetZoneDescriptor> zones;
    QDateTime loadedAt;

    bool isValid() const;
    bool hasUniqueZoneIds() const;
    const FieldDescriptor *findField(const QString &key) const;
    const DatasetZoneDescriptor *findZone(const QString &zoneId) const;
};

enum class SceneObjectKind {
    Unknown, SourceMesh, Slice, IsoSurface, Contour, Streamline, Pathline,
    Surface, Curve, Annotation
};

// Display attributes are kept apart from the raw result data they describe.
struct SceneObjectStyle {
    bool visible = true;
    RgbaColor color;
    int opacityPercent = 100;
    MeshRepresentation representation = MeshRepresentation::Surface;
    bool showColorMap = false;
    double lineWidth = 1.0;

    bool isValid() const;
};

// A post-processing scene object. It references an existing dataset instead of
// copying data, and carries its own style revision.
struct SceneObjectDescriptor {
    SceneObjectRef ref;
    DatasetRef dataset;
    QString name;
    SceneObjectKind kind = SceneObjectKind::Unknown;
    MeshRefV2 sourceMesh;       // optional; required for spatial features
    SceneObjectStyle style;
    quint64 styleRevision = 0;

    bool isValid() const;
    bool requiresSourceMesh() const;
};

// Scene document persisted with the project. It references existing results and
// never owns VTK actors or renderers.
struct PostProcessScene {
    QString projectId;
    QString sceneId;
    QString name;
    quint64 revision = 0;
    quint64 schemaVersion = 0;
    QVector<SceneObjectDescriptor> objects;

    bool isValid() const;
    bool hasUniqueObjectIds() const;
    const SceneObjectDescriptor *findObject(const QString &sceneObjectId) const;
};

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::FieldDescriptor)
Q_DECLARE_METATYPE(aerosim::DatasetZoneDescriptor)
Q_DECLARE_METATYPE(aerosim::ResultDescriptor)
Q_DECLARE_METATYPE(aerosim::DatasetDescriptor)
Q_DECLARE_METATYPE(aerosim::SceneObjectStyle)
Q_DECLARE_METATYPE(aerosim::SceneObjectDescriptor)
Q_DECLARE_METATYPE(aerosim::PostProcessScene)
