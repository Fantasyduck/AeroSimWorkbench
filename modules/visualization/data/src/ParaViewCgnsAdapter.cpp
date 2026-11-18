#include "aerosim/visualization/DataAdapter.h"
#include "vtkCGNSReader.h"
#include "vtkCGNSSubsetInclusionLattice.h"
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <cgnslib.h>
#include <vtkCallbackCommand.h>
#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkCommand.h>
#include <vtkCompositeDataSet.h>
#include <vtkDataArray.h>
#include <vtkExecutive.h>
#include <vtkExtractCells.h>
#include <vtkIdList.h>
#include <vtkInformation.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkStructuredGrid.h>
#include <vtkUnstructuredGrid.h>
#include <cmath>
#include <vector>
#include <set>
#include <algorithm>

namespace aerosim { namespace visualization {
namespace {
struct ReaderErrors { bool failed = false; QString message; };
void onReaderError(vtkObject *, unsigned long, void *context, void *message) {
    auto *error = static_cast<ReaderErrors *>(context);
    error->failed = true;
    if (message) error->message = QString::fromLocal8Bit(static_cast<const char *>(message)).trimmed();
}

void onReaderWarning(vtkObject *, unsigned long, void *context, void *message) {
    if (!message) return;
    const QString warning = QString::fromLocal8Bit(static_cast<const char *>(message)).trimmed();
    if (warning.contains(QStringLiteral("Skipping BC_t node"))) {
        auto *error = static_cast<ReaderErrors *>(context);
        error->failed = true;
        error->message = warning;
    }
}

// ParaView patches share the entire zone's point array. Compact only patch
// surfaces so bounds and per-patch field ranges use referenced points. Point
// and cell tuples are copied by ID, not merged by coordinates.
vtkSmartPointer<vtkDataSet> compactPatch(vtkDataSet *input) {
    if (!input || input->GetNumberOfCells() == 0) return input;
    for (vtkIdType i = 0; i < input->GetNumberOfCells(); ++i)
        if (input->GetCell(i)->GetCellDimension() > 2) return input;
    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    auto points = vtkSmartPointer<vtkPoints>::New(); points->SetDataTypeToDouble();
    grid->SetPoints(points); grid->Allocate(input->GetNumberOfCells());
    auto *sourcePoints = input->GetPointData(); auto *targetPoints = grid->GetPointData();
    auto *sourceCells = input->GetCellData(); auto *targetCells = grid->GetCellData();
    targetPoints->CopyGlobalIdsOn(); targetPoints->CopyAllocate(sourcePoints);
    targetCells->CopyGlobalIdsOn(); targetCells->CopyAllocate(sourceCells, input->GetNumberOfCells());
    std::vector<vtkIdType> mapping(static_cast<size_t>(input->GetNumberOfPoints()), -1);
    vtkNew<vtkIdList> ids;
    for (vtkIdType ci = 0; ci < input->GetNumberOfCells(); ++ci) {
        vtkCell *cell = input->GetCell(ci); ids->Reset();
        for (vtkIdType j = 0; j < cell->GetNumberOfPoints(); ++j) {
            const vtkIdType source = cell->GetPointId(j);
            if (source < 0 || source >= input->GetNumberOfPoints()) return nullptr;
            auto &target = mapping[static_cast<size_t>(source)];
            if (target < 0) { target = points->InsertNextPoint(input->GetPoint(source)); targetPoints->CopyData(sourcePoints, source, target); }
            ids->InsertNextId(target);
        }
        const vtkIdType targetCell = grid->InsertNextCell(cell->GetCellType(), ids);
        targetCells->CopyData(sourceCells, ci, targetCell);
    }
    return grid;
}
void appendFields(RegionData &region, vtkDataSetAttributes *attributes, const QString &association) {
    for (int i = 0; i < attributes->GetNumberOfArrays(); ++i) {
        vtkDataArray *array = attributes->GetArray(i);
        if (!array || !array->GetName() || array->GetNumberOfTuples() == 0) continue;
        FieldInfo field; field.name = QString::fromUtf8(array->GetName());
        field.association = association; field.components = array->GetNumberOfComponents();
        double range[2]; array->GetRange(range, field.components == 1 ? 0 : -1);
        field.hasRange = std::isfinite(range[0]) && std::isfinite(range[1]) && range[0] <= range[1];
        if (field.hasRange) { field.minimum = range[0]; field.maximum = range[1]; }
        region.fields.append(field);
    }
}
struct Boundary { QString name; std::set<cgsize_t> elements; };
struct Section { cgsize_t first = 0; cgsize_t last = 0; QVector<Boundary> boundaries; bool processed = false; };
using Sections = QMap<QString, Section>;
QString sectionKey(const QString &base, const QString &zone, const QString &section) {
    return base + QChar(0x1f) + zone + QChar(0x1f) + section;
}
// Unstructured ParaView 5.6 exposes boundary element sections. Restore
// FaceCenter ZoneBC names and groupings from CGNS element IDs when possible.
bool readBoundaryMetadata(const QByteArray &path, Sections &sections, QString &error) {
    int file = 0;
    if (cg_open(path.constData(), CG_MODE_READ, &file) != CG_OK) { error = QString::fromLocal8Bit(cg_get_error()); return false; }
    struct FileCloser { int file; ~FileCloser() { cg_close(file); } } closer{file};
    int bases = 0; if (cg_nbases(file, &bases) != CG_OK) { error = QString::fromLocal8Bit(cg_get_error()); return false; }
    for (int b=1;b<=bases;++b) {
        char base[33]={}; int cellDim=0, physicalDim=0, zones=0;
        if (cg_base_read(file,b,base,&cellDim,&physicalDim)!=CG_OK || cg_nzones(file,b,&zones)!=CG_OK) { error=QString::fromLocal8Bit(cg_get_error()); return false; }
        for (int z=1;z<=zones;++z) {
            char zone[33]={}; cgsize_t sizes[9]={}; CGNS_ENUMT(ZoneType_t) zoneType;
            if (cg_zone_read(file,b,z,zone,sizes)!=CG_OK || cg_zone_type(file,b,z,&zoneType)!=CG_OK) { error=QString::fromLocal8Bit(cg_get_error()); return false; }
            if (zoneType != CGNS_ENUMV(Unstructured)) continue;
            int count=0; if (cg_nsections(file,b,z,&count)!=CG_OK) { error=QString::fromLocal8Bit(cg_get_error()); return false; }
            QVector<QString> sectionKeys;
            for (int n=1;n<=count;++n) {
                char name[33]={}; CGNS_ENUMT(ElementType_t) type; cgsize_t first=0,last=0; int boundary=0,parent=0;
                if (cg_section_read(file,b,z,n,name,&type,&first,&last,&boundary,&parent)!=CG_OK) { error=QString::fromLocal8Bit(cg_get_error()); return false; }
                const QString key=sectionKey(QString::fromUtf8(base),QString::fromUtf8(zone),QString::fromUtf8(name));
                Section section; section.first=first; section.last=last; sections.insert(key,section); sectionKeys.append(key);
            }
            int bcs=0; if(cg_nbocos(file,b,z,&bcs)!=CG_OK) { error=QString::fromLocal8Bit(cg_get_error()); return false; }
            for (int n=1;n<=bcs;++n) {
                char name[33]={}; CGNS_ENUMT(BCType_t) type; CGNS_ENUMT(PointSetType_t) setType; CGNS_ENUMT(DataType_t) normalType;
                cgsize_t countPoints=0, normalSize=0; int normalIndex[3]={}, dataSets=0;
                if(cg_boco_info(file,b,z,n,name,&type,&setType,&countPoints,normalIndex,&normalSize,&normalType,&dataSets)!=CG_OK) { error=QString::fromLocal8Bit(cg_get_error()); return false; }
                CGNS_ENUMT(GridLocation_t) location=CGNS_ENUMV(Vertex);
                if(cg_boco_gridlocation_read(file,b,z,n,&location)!=CG_OK) { error=QString::fromLocal8Bit(cg_get_error()); return false; }
                const bool range=setType==CGNS_ENUMV(PointRange) || setType==CGNS_ENUMV(ElementRange);
                const bool list=setType==CGNS_ENUMV(PointList) || setType==CGNS_ENUMV(ElementList);
                if ((location!=CGNS_ENUMV(FaceCenter) && setType!=CGNS_ENUMV(ElementRange) && setType!=CGNS_ENUMV(ElementList)) || (!range && !list) || countPoints<=0 || (range && countPoints!=2)) {
                    error=QStringLiteral("Unsupported CGNS boundary point set or location: %1 / %2 / %3").arg(QString::fromUtf8(base), QString::fromUtf8(zone), QString::fromUtf8(name));
                    return false;
                }
                std::vector<cgsize_t> ids(static_cast<size_t>(countPoints)); std::vector<double> normals(static_cast<size_t>(normalSize));
                if(cg_boco_read(file,b,z,n,ids.data(),normals.empty()?nullptr:normals.data())!=CG_OK) { error=QString::fromLocal8Bit(cg_get_error()); return false; }
                if (ids[0] < 1 || (range && ids[1] < 1)) {
                    error=QStringLiteral("CGNS boundary '%1' contains invalid element IDs.").arg(QString::fromUtf8(name));
                    return false;
                }
                cgsize_t matchedCount = 0;
                for (const QString &key: sectionKeys) {
                    auto &section=sections[key]; Boundary boundary; boundary.name=QString::fromUtf8(name);
                    if (range) {
                        const cgsize_t low=std::max(section.first,std::min(ids[0],ids[1]));
                        const cgsize_t high=std::min(section.last,std::max(ids[0],ids[1]));
                        if (low <= high) for(cgsize_t v=low;;++v) { boundary.elements.insert(v); if (v==high) break; }
                    } else for(cgsize_t v:ids) if(v>=section.first && v<=section.last) boundary.elements.insert(v);
                    if (!boundary.elements.empty()) {
                        matchedCount += static_cast<cgsize_t>(boundary.elements.size());
                        section.boundaries.append(boundary);
                    }
                }
                const cgsize_t expectedCount = range ? (std::max(ids[0],ids[1]) - std::min(ids[0],ids[1]) + 1) : countPoints;
                if (matchedCount != expectedCount) {
                    error=QStringLiteral("CGNS boundary '%1' contains invalid, duplicate, or unmapped element IDs.").arg(QString::fromUtf8(name));
                    return false;
                }
            }
        }
    }
    return true;
}
bool collect(vtkDataObject *object, const QStringList &path, const QString &id,
             bool patch, Sections &sections, ImportedData &out, QString &error, bool classified = false) {
    if (!object) return true;
    if (auto *blocks = vtkMultiBlockDataSet::SafeDownCast(object)) {
        for (unsigned i = 0; i < blocks->GetNumberOfBlocks(); ++i) {
            QString label = QStringLiteral("Block %1").arg(i + 1);
            if (blocks->HasMetaData(i) && blocks->GetMetaData(i)->Has(vtkCompositeDataSet::NAME()))
                label = QString::fromUtf8(blocks->GetMetaData(i)->Get(vtkCompositeDataSet::NAME()));
            if (!collect(blocks->GetBlock(i), path + QStringList{label}, id + QStringLiteral("/") + QString::number(i),
                         patch || label == QStringLiteral("Patches"), sections, out, error, classified)) return false;
        }
        return true;
    }
    auto *data = vtkDataSet::SafeDownCast(object);
    if (!data) { error = QStringLiteral("Unsupported CGNS reader output: ") + QString::fromLatin1(object->GetClassName()); return false; }
    if (data->GetNumberOfCells() == 0) return true;
    if (patch && !classified && path.size() >= 4) {
        const QString key=sectionKey(path.at(0),path.at(1),path.last());
        auto found=sections.find(key);
        if (found != sections.end() && !found->boundaries.isEmpty()) {
            if (found->last - found->first + 1 != static_cast<cgsize_t>(data->GetNumberOfCells())) {
                error = QStringLiteral("Cannot map CGNS boundary elements to section: ") + path.join(QStringLiteral(" / "));
                return false;
            }
            found->processed = true;
            std::set<vtkIdType> used;
            for (int bi=0;bi<found->boundaries.size();++bi) {
                const auto &boundary=found->boundaries[bi]; vtkNew<vtkIdList> cellIds;
                for (cgsize_t element:boundary.elements) { const vtkIdType ci=static_cast<vtkIdType>(element-found->first); cellIds->InsertNextId(ci); used.insert(ci); }
                vtkNew<vtkExtractCells> extract; extract->SetInputData(data); extract->SetCellList(cellIds); extract->Update();
                vtkDataSet::SafeDownCast(extract->GetOutputDataObject(0))->GetCellData()->RemoveArray("vtkOriginalCellIds");
                if (!collect(extract->GetOutputDataObject(0), path.mid(0,2) + QStringList{QStringLiteral("Patches"),boundary.name}, id + QStringLiteral("/bc/") + QString::number(bi), true, sections, out, error, true)) return false;
            }
            if (used.size() < static_cast<size_t>(data->GetNumberOfCells())) {
                vtkNew<vtkIdList> rest; for(vtkIdType i=0;i<data->GetNumberOfCells();++i) if(!used.count(i)) rest->InsertNextId(i);
                vtkNew<vtkExtractCells> extract; extract->SetInputData(data); extract->SetCellList(rest); extract->Update();
                vtkDataSet::SafeDownCast(extract->GetOutputDataObject(0))->GetCellData()->RemoveArray("vtkOriginalCellIds");
                Sections noSections;
                if (!collect(extract->GetOutputDataObject(0), path, id + QStringLiteral("/unclassified"), true, noSections, out, error)) return false;
            }
            return true;
        }
    }
    RegionData region; region.id = id; region.name = path.join(QStringLiteral(" / "));
    region.kind = patch ? ((classified || vtkStructuredGrid::SafeDownCast(data)) ? QStringLiteral("boundary") : QStringLiteral("section")) : QStringLiteral("volume");
    region.dimension = 0;
    for (vtkIdType i = 0; i < data->GetNumberOfCells(); ++i)
        region.dimension = qMax(region.dimension, data->GetCell(i)->GetCellDimension());
    region.data = patch ? compactPatch(data) : vtkSmartPointer<vtkDataSet>(data);
    if (!region.data) { error = QStringLiteral("Invalid patch connectivity: ") + region.name; return false; }
    appendFields(region, region.data->GetPointData(), QStringLiteral("point"));
    appendFields(region, region.data->GetCellData(), QStringLiteral("cell"));
    out.regions.append(region); return true;
}
class ParaViewCgnsAdapter final : public DataAdapter {
public:
    bool accepts(const QString &path) const override { return path.endsWith(QStringLiteral(".cgns"), Qt::CaseInsensitive); }
    ImportResult read(const QString &path) const override {
        ImportResult result; result.dataset.path = path;
        if (!QFileInfo(path).isFile()) { result.error = QStringLiteral("CGNS file does not exist: ") + path; return result; }
        const QByteArray encoded = QFile::encodeName(path);
        if (QFile::decodeName(encoded) != path) { result.error = QStringLiteral("CGNS path encoding is not supported"); return result; }
        ReaderErrors errors; vtkNew<vtkCallbackCommand> callback;
        callback->SetClientData(&errors); callback->SetCallback(onReaderError);
        vtkNew<vtkCGNSReader> reader;
        reader->AddObserver(vtkCommand::ErrorEvent, callback);
        reader->GetExecutive()->AddObserver(vtkCommand::ErrorEvent, callback);
        vtkNew<vtkCallbackCommand> warningCallback;
        warningCallback->SetClientData(&errors); warningCallback->SetCallback(onReaderWarning);
        reader->AddObserver(vtkCommand::WarningEvent, warningCallback);
        reader->DistributeBlocksOff(); // This standalone workbench must load every zone.
        reader->SetFileName(encoded.constData()); reader->UpdateInformation();
        if (errors.failed) { result.error = QStringLiteral("CGNS metadata: ") + errors.message; return result; }
        vtkInformation *info = reader->GetOutputInformation(0);
        if (info && info->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS()) &&
            info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS()) > 1) {
            result.error = QStringLiteral("CGNS time series are not yet supported by this experimental backend"); return result;
        }
        reader->EnableAllFamilies();
        reader->GetSIL()->Select("/Patches");
        // Patch selections can deselect grids in the shared SIL; select grids last.
        reader->EnableAllBases();
        // The historical SIL can report a selected parent while a child Grid
        // remains unselected. Select each actual Grid node explicitly.
        auto *sil = reader->GetSIL();
        for (int baseNode : sil->GetChildren(sil->FindNode("/Hierarchy")))
            for (int zoneNode : sil->GetChildren(baseNode))
                for (int child : sil->GetChildren(zoneNode))
                    if (QString::fromUtf8(sil->GetNodeName(child)) == QStringLiteral("Grid")) sil->Select(child);
        reader->EnableAllPointArrays(); reader->EnableAllCellArrays(); reader->Update();
        if (errors.failed) { result.error = QStringLiteral("CGNS read: ") + errors.message; return result; }
        Sections sections;
        if (!readBoundaryMetadata(encoded, sections, result.error)) return result;
        if (!collect(reader->GetOutputDataObject(0), {}, QStringLiteral("root"), false, sections, result.dataset, result.error)) {
            result.dataset.regions.clear(); return result;
        }
        for (auto it = sections.cbegin(); it != sections.cend(); ++it) {
            if (!it->boundaries.isEmpty() && !it->processed) {
                result.dataset.regions.clear();
                result.error = QStringLiteral("CGNS boundary '%1' references elements that cannot be mapped to a boundary surface (section %2).")
                    .arg(it->boundaries.first().name, it.key().split(QChar(0x1f)).join(QStringLiteral(" / ")));
                return result;
            }
        }
        if (result.dataset.regions.isEmpty()) result.error = QStringLiteral("CGNS contains no readable regions");
        return result;
    }
};
}
std::unique_ptr<DataAdapter> createParaViewCgnsAdapter() { return std::unique_ptr<DataAdapter>(new ParaViewCgnsAdapter); }
} }
