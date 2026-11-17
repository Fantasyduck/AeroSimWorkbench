#pragma once
#include <vtkSmartPointer.h>
#include <vtkDataObject.h>
#include <vtkDataSet.h>
#include <vtkPolyData.h>
#include <QString>
#include <atomic>
#include <memory>
namespace aerosim { namespace visualization {
// Module-local immutable snapshot, not a new public mesh contract.
struct BoundaryMesh {
    vtkSmartPointer<vtkDataObject> source;
    vtkSmartPointer<vtkPolyData> surface;
    vtkIdType inputPoints=0, inputCells=0, boundaryFaces=0, uniqueBoundaryPoints=0;
    bool volume=false;
    QString summary() const;
};
class BoundarySurfaceExtractor {
public:
    // Caller owns input exclusively until completion, then treats it as immutable.
    static std::shared_ptr<const BoundaryMesh> extract(vtkDataSet* input,
        const std::atomic_bool* cancelled=nullptr);
};
} }
