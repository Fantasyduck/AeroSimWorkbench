#pragma once

#include <QString>
#include <QtGlobal>

namespace aerosim {

struct ProjectRef {
    QString projectId;
    QString filePath;
    quint64 revision = 0;
};

// IDs and paths are copied across modules. They do not own the referenced data.
struct ResourceRef {
    QString resourceId;
    QString projectId;
    QString kind;
    QString filePath;
    quint64 revision = 0;
};

struct ScopeRef {
    QString projectId;
    QString caseId;
    QString objectId;
};

struct ParameterSnapshotRef {
    QString snapshotId;
    ScopeRef scope;
    quint64 revision = 0;
};

struct MeshRef {
    ResourceRef resource;
    QString meshVersion;
};

struct ResultRef {
    ResourceRef resource;
    QString taskId;
    QString meshVersion;
    QString caseId;
};

} // namespace aerosim
