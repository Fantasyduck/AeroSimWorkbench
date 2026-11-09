#pragma once

#include "aerosim/contracts/References.h"

#include <QMetaType>
#include <QString>
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

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::MeshRefV2)
