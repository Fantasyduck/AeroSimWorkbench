#pragma once

#include "FeatureProcessor.h"
#include <QString>

namespace aerosim {
namespace visualization {

struct ExportOptions {
    // Empty means retain all numeric/string data arrays. Geometry is always kept.
    std::vector<std::string> pointArrays;
    std::vector<std::string> cellArrays;
    bool overwrite = false;
};

class FeatureExporter {
public:
    // Writes actual VTK XML PolyData only. Atomic commit through QSaveFile;
    // failures do not truncate an existing output. UTF-8/Unicode paths supported.
    static FeatureStatus writeVtp(const FeatureGeometry &geometry, const QString &path,
                                  const ExportOptions &options = ExportOptions());
};

} // namespace visualization
} // namespace aerosim
