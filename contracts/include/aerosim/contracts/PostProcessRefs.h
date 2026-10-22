#pragma once

#include <QMetaType>
#include <QString>

namespace aerosim {

// Additive identity-only references. Dataset contents and VTK objects remain
// owned by the visualization module; IDs do not transfer ownership.
struct DatasetRef {
    QString datasetId;
    QString fileId;
};

struct SceneObjectRef {
    QString sceneObjectId;
    QString datasetId;
};

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::DatasetRef)
Q_DECLARE_METATYPE(aerosim::SceneObjectRef)
