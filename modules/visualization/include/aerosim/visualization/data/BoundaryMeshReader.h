#pragma once
#include "aerosim/visualization/features/BoundarySurfaceExtractor.h"
namespace aerosim { namespace visualization {
class BoundaryMeshReader {
public:
    static std::shared_ptr<const BoundaryMesh> read(const QString& path,
        const std::atomic_bool* cancelled=nullptr);
};
} }
