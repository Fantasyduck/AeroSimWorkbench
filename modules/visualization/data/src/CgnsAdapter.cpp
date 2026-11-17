#if defined(_MSC_VER) && !defined(_DEBUG)
#error The current CGNS/VTK integration is verified for Windows Debug only.
#endif
#include "aerosim/visualization/DataAdapter.h"
#include <QFile>
#include <cgnslib.h>
#include <vtkCellType.h>
#include <vtkCellData.h>
#include <vtkCell.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkUnstructuredGrid.h>
#include <vtkStructuredGrid.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <unordered_set>

namespace aerosim { namespace visualization {
namespace {
struct CgnsFile { int id = 0; ~CgnsFile() { if (id) cg_close(id); } };
QString reason(const QString &prefix) { return prefix + QStringLiteral(": ") + QString::fromLocal8Bit(cg_get_error()); }
int vtkType(CGNS_ENUMT(ElementType_t) type) {
    switch (type) {
    case CGNS_ENUMV(NODE): return VTK_VERTEX;
    case CGNS_ENUMV(BAR_2): return VTK_LINE;
    case CGNS_ENUMV(TRI_3): return VTK_TRIANGLE;
    case CGNS_ENUMV(QUAD_4): return VTK_QUAD;
    case CGNS_ENUMV(TETRA_4): return VTK_TETRA;
    case CGNS_ENUMV(PYRA_5): return VTK_PYRAMID;
    case CGNS_ENUMV(PENTA_6): return VTK_WEDGE;
    case CGNS_ENUMV(HEXA_8): return VTK_HEXAHEDRON;
    default: return -1;
    }
}
int dimension(CGNS_ENUMT(ElementType_t) type) {
    if (type == CGNS_ENUMV(TRI_3) || type == CGNS_ENUMV(QUAD_4)) return 2;
    if (type == CGNS_ENUMV(BAR_2)) return 1;
    if (type == CGNS_ENUMV(NODE)) return 0;
    return 3;
}
class CgnsAdapter final : public DataAdapter {
public:
    bool accepts(const QString &path) const override { return path.endsWith(QStringLiteral(".cgns"), Qt::CaseInsensitive); }
    ImportResult read(const QString &path) const override {
        ImportResult result; result.dataset.path = path;
        CgnsFile file;
        if (cg_open(QFile::encodeName(path).constData(), CG_MODE_READ, &file.id) != CG_OK) { file.id = 0; result.error = reason(QStringLiteral("CGNS open failed")); return result; }
        int bases = 0; if (cg_nbases(file.id, &bases) != CG_OK) { result.error = reason(QStringLiteral("CGNS bases")); return result; }
        for (int b = 1; b <= bases; ++b) {
            char baseName[33] = {}; int cellDim = 0, physicalDim = 0;
            if (cg_base_read(file.id, b, baseName, &cellDim, &physicalDim) != CG_OK) { result.error = reason(QStringLiteral("CGNS base")); return result; }
            int zones = 0; if (cg_nzones(file.id, b, &zones) != CG_OK) { result.error = reason(QStringLiteral("CGNS zones")); return result; }
            for (int z = 1; z <= zones; ++z) {
                char zoneName[33] = {}; cgsize_t sizes[9] = {};
                CGNS_ENUMT(ZoneType_t) zoneType;
                if (cg_zone_read(file.id,b,z,zoneName,sizes) != CG_OK || cg_zone_type(file.id,b,z,&zoneType) != CG_OK) { result.error = reason(QStringLiteral("CGNS zone")); return result; }
                if (cellDim != 3 || physicalDim != 3) { result.error = QStringLiteral("Only 3D CGNS datasets are currently supported"); return result; }
                if (zoneType != CGNS_ENUMV(Unstructured)) { result.error = QStringLiteral("Unsupported CGNS zone type (only unstructured zones are currently supported): ") + QString::fromLocal8Bit(zoneName); return result; }
                const cgsize_t count = sizes[0];
                if (count <= 0 || sizes[1] < 0) { result.error = QStringLiteral("Invalid CGNS zone size"); return result; }
                cgsize_t lo[3] = {1,1,1}, hi[3] = {count,1,1};
                std::array<std::vector<double>,3> xyz;
                const char *coordinates[] = {"CoordinateX","CoordinateY","CoordinateZ"};
                for (int i=0;i<3;++i) { xyz[i].assign(static_cast<size_t>(count),0.0); if (i < physicalDim && cg_coord_read(file.id,b,z,coordinates[i],CGNS_ENUMV(RealDouble),lo,hi,xyz[i].data()) != CG_OK) { result.error=reason(QStringLiteral("CGNS coordinates")); return result; } }
                auto points = vtkSmartPointer<vtkPoints>::New(); points->SetDataTypeToDouble(); points->SetNumberOfPoints(static_cast<vtkIdType>(count));
                for (cgsize_t i=0;i<count;++i) points->SetPoint(static_cast<vtkIdType>(i), xyz[0][i], xyz[1][i], xyz[2][i]);
                auto volume = vtkSmartPointer<vtkUnstructuredGrid>::New(); volume->SetPoints(points);
                QVector<RegionData> surfaces;
                QVector<QVector<cgsize_t>> surfacePointIds;
                QVector<cgsize_t> surfaceFirst;
                int sections=0; if (cg_nsections(file.id,b,z,&sections) != CG_OK) { result.error=reason(QStringLiteral("CGNS sections")); return result; }
                // Element sections need not be stored in element-number order.
                // Process by range so cell-centered arrays retain their CGNS ordering.
                std::vector<std::pair<cgsize_t,int>> sectionOrder;
                for(int n=1;n<=sections;++n) {
                    char name[33]={}; CGNS_ENUMT(ElementType_t) type; cgsize_t first=0,last=0; int boundary=0,parent=0;
                    if(cg_section_read(file.id,b,z,n,name,&type,&first,&last,&boundary,&parent)!=CG_OK) { result.error=reason(QStringLiteral("CGNS section")); return result; }
                    if(first<1 || last<first) { result.error=QStringLiteral("Invalid CGNS element section range"); return result; }
                    sectionOrder.emplace_back(first,n);
                }
                std::sort(sectionOrder.begin(),sectionOrder.end());
                cgsize_t nextVolumeElement = 1;
                for(const auto &entry:sectionOrder) {
                    const int n=entry.second;
                    char name[33]={}; CGNS_ENUMT(ElementType_t) type; cgsize_t first=0,last=0; int boundary=0,parent=0;
                    if(cg_section_read(file.id,b,z,n,name,&type,&first,&last,&boundary,&parent)!=CG_OK) { result.error=reason(QStringLiteral("CGNS section")); return result; }
                    int cellType=vtkType(type), nodes=0; if(cellType<0 || cg_npe(type,&nodes)!=CG_OK || nodes<=0) { result.error=QStringLiteral("Unsupported CGNS element section: ")+QString::fromLocal8Bit(name); return result; }
                    cgsize_t dataSize=0; if(cg_ElementDataSize(file.id,b,z,n,&dataSize)!=CG_OK || dataSize != (last-first+1)*nodes) { result.error=reason(QStringLiteral("CGNS connectivity size")); return result; }
                    std::vector<cgsize_t> connectivity(static_cast<size_t>(dataSize));
                    if(cg_elements_read(file.id,b,z,n,connectivity.data(),nullptr)!=CG_OK) { result.error=reason(QStringLiteral("CGNS connectivity")); return result; }
                    const bool isBoundary=dimension(type)<cellDim;
                    if (!isBoundary) {
                        if (first != nextVolumeElement) { result.error=QStringLiteral("CGNS volume sections must be contiguous and in element order"); return result; }
                        nextVolumeElement=last+1;
                    }
                    auto grid = isBoundary ? vtkSmartPointer<vtkUnstructuredGrid>::New() : volume;
                    vtkSmartPointer<vtkPoints> boundaryPoints;
                    std::vector<vtkIdType> localIds;
                    QVector<cgsize_t> sourcePointIds;
                    if(isBoundary) {
                        boundaryPoints=vtkSmartPointer<vtkPoints>::New(); boundaryPoints->SetDataTypeToDouble();
                        grid->SetPoints(boundaryPoints); localIds.assign(static_cast<size_t>(count),-1);
                    }
                    std::vector<vtkIdType> ids(static_cast<size_t>(nodes));
                    for(cgsize_t i=0;i<=last-first;++i) {
                        for(int k=0;k<nodes;++k) { const cgsize_t id=connectivity[static_cast<size_t>(i*nodes+k)]; if(id<1 || id>count) { result.error=QStringLiteral("Invalid CGNS connectivity index"); return result; } if(isBoundary) {
                            auto &local=localIds[static_cast<size_t>(id-1)];
                            if(local<0) { local=boundaryPoints->InsertNextPoint(xyz[0][id-1],xyz[1][id-1],xyz[2][id-1]); sourcePointIds.append(id-1); }
                            ids[k]=local;
                        } else ids[k]=static_cast<vtkIdType>(id-1); }
                        grid->InsertNextCell(cellType,nodes,ids.data());
                    }
                    if(isBoundary) { RegionData region; region.name=QString::fromLocal8Bit(name); region.id=QStringLiteral("%1/%2/%3").arg(b).arg(z).arg(n); region.dimension=dimension(type); region.kind=QStringLiteral("section"); region.data=grid; surfaces.append(region); surfacePointIds.append(sourcePointIds); surfaceFirst.append(first); }
                }
                // Resolve face-centred ZoneBC element ranges/lists against element sections.
                // Keep unmatched sections explicitly marked as sections, never invent a BC.
                const auto sectionsData=surfaces;
                const auto sectionPointIds=surfacePointIds;
                QVector<QVector<bool>> used;
                for(const auto &part:sectionsData) used.append(QVector<bool>(static_cast<int>(part.data->GetNumberOfCells()),false));
                surfaces.clear(); surfacePointIds.clear();
                auto makeRegion = [&](const QString &name,const QString &id,const QString &kind,
                                      const QVector<QVector<vtkIdType>> &selected) {
                    auto grid=vtkSmartPointer<vtkUnstructuredGrid>::New();
                    auto boundaryPoints=vtkSmartPointer<vtkPoints>::New(); boundaryPoints->SetDataTypeToDouble(); grid->SetPoints(boundaryPoints);
                    std::vector<vtkIdType> localIds(static_cast<size_t>(count),-1); QVector<cgsize_t> sourcePointIds;
                    int regionDimension=2;
                    for(int si=0;si<selected.size();++si) for(vtkIdType ci:selected[si]) {
                        vtkCell *cell=sectionsData[si].data->GetCell(ci); regionDimension=sectionsData[si].dimension;
                        std::vector<vtkIdType> ids(static_cast<size_t>(cell->GetNumberOfPoints()));
                        for(vtkIdType pi=0;pi<cell->GetNumberOfPoints();++pi) {
                            const cgsize_t source=sectionPointIds[si][static_cast<int>(cell->GetPointId(pi))];
                            auto &local=localIds[static_cast<size_t>(source)];
                            if(local<0) { local=boundaryPoints->InsertNextPoint(xyz[0][source],xyz[1][source],xyz[2][source]); sourcePointIds.append(source); }
                            ids[static_cast<size_t>(pi)]=local;
                        }
                        grid->InsertNextCell(cell->GetCellType(),static_cast<vtkIdType>(ids.size()),ids.data());
                    }
                    if(grid->GetNumberOfCells()==0) return;
                    RegionData part; part.name=name; part.id=id; part.kind=kind; part.dimension=regionDimension; part.data=grid;
                    surfaces.append(part); surfacePointIds.append(sourcePointIds);
                };
                int bcs=0; if(cg_nbocos(file.id,b,z,&bcs)!=CG_OK) { result.error=reason(QStringLiteral("CGNS boundary conditions")); return result; }
                for(int bc=1;bc<=bcs;++bc) {
                    char name[33]={}; CGNS_ENUMT(BCType_t) bcType; CGNS_ENUMT(PointSetType_t) setType; CGNS_ENUMT(DataType_t) normalType;
                    cgsize_t npoints=0, normalSize=0; int normalIndex[3]={}, ndatasets=0;
                    if(cg_boco_info(file.id,b,z,bc,name,&bcType,&setType,&npoints,normalIndex,&normalSize,&normalType,&ndatasets)!=CG_OK) { result.error=reason(QStringLiteral("CGNS boundary metadata")); return result; }
                    CGNS_ENUMT(GridLocation_t) location=CGNS_ENUMV(Vertex);
                    if(cg_boco_gridlocation_read(file.id,b,z,bc,&location)!=CG_OK) { result.error=reason(QStringLiteral("CGNS boundary location")); return result; }
                    const bool range=setType==CGNS_ENUMV(PointRange) || setType==CGNS_ENUMV(ElementRange);
                    const bool list=setType==CGNS_ENUMV(PointList) || setType==CGNS_ENUMV(ElementList);
                    if((location!=CGNS_ENUMV(FaceCenter) && setType!=CGNS_ENUMV(ElementRange) && setType!=CGNS_ENUMV(ElementList)) || (!range && !list) || npoints<=0 || (range && npoints!=2)) continue;
                    std::vector<cgsize_t> indices(static_cast<size_t>(npoints)); std::vector<double> normals(static_cast<size_t>(normalSize));
                    if(cg_boco_read(file.id,b,z,bc,indices.data(),normals.empty()?nullptr:normals.data())!=CG_OK) { result.error=reason(QStringLiteral("CGNS boundary indices")); return result; }
                    std::unordered_set<cgsize_t> indexSet; if(list) indexSet.insert(indices.begin(),indices.end());
                    const cgsize_t first=range?std::min(indices[0],indices[1]):0, last=range?std::max(indices[0],indices[1]):0;
                    QVector<QVector<vtkIdType>> selected(sectionsData.size());
                    for(int si=0;si<sectionsData.size();++si) {
                        if(sectionsData[si].dimension!=2) continue;
                        for(vtkIdType ci=0;ci<sectionsData[si].data->GetNumberOfCells();++ci) {
                            const cgsize_t element=surfaceFirst[si]+static_cast<cgsize_t>(ci);
                            if((range && element>=first && element<=last) || (list && indexSet.count(element))) { selected[si].append(ci); used[si][static_cast<int>(ci)]=true; }
                        }
                    }
                    makeRegion(QString::fromLocal8Bit(name),QStringLiteral("%1/%2/bc/%3").arg(b).arg(z).arg(bc),QStringLiteral("boundary"),selected);
                }
                for(int si=0;si<sectionsData.size();++si) {
                    QVector<QVector<vtkIdType>> selected(sectionsData.size());
                    for(vtkIdType ci=0;ci<sectionsData[si].data->GetNumberOfCells();++ci) if(!used[si][static_cast<int>(ci)]) selected[si].append(ci);
                    makeRegion(sectionsData[si].name,sectionsData[si].id,QStringLiteral("section"),selected);
                }
                RegionData region; region.name=QString::fromLocal8Bit(zoneName); region.id=QStringLiteral("%1/%2").arg(b).arg(z); region.dimension=cellDim; region.kind=QStringLiteral("volume"); region.data=volume;
                int solutions=0; if(cg_nsols(file.id,b,z,&solutions)!=CG_OK) { result.error=reason(QStringLiteral("CGNS solutions")); return result; }
                if(solutions>1) { result.error=QStringLiteral("Multiple CGNS solutions / time steps are not yet supported"); return result; }
                for(int sol=1;sol<=solutions;++sol) {
                    char solName[33]={}; CGNS_ENUMT(GridLocation_t) location;
                    if(cg_sol_info(file.id,b,z,sol,solName,&location)!=CG_OK) { result.error=reason(QStringLiteral("CGNS solution")); return result; }
                    if(location!=CGNS_ENUMV(Vertex) && location!=CGNS_ENUMV(CellCenter)) { result.error=QStringLiteral("Unsupported CGNS field location"); return result; }
                    const bool point=location==CGNS_ENUMV(Vertex); cgsize_t upper[3]={point?count:sizes[1],1,1}; int fields=0;
                    if(cg_nfields(file.id,b,z,sol,&fields)!=CG_OK) { result.error=reason(QStringLiteral("CGNS fields")); return result; }
                    if(!point && volume->GetNumberOfCells()!=static_cast<vtkIdType>(sizes[1])) { result.error=QStringLiteral("Cell fields do not match CGNS volume sections"); return result; }
                    for(int f=1;f<=fields;++f) {
                        char name[33]={}; CGNS_ENUMT(DataType_t) datatype;
                        if(cg_field_info(file.id,b,z,sol,f,&datatype,name)!=CG_OK) { result.error=reason(QStringLiteral("CGNS field metadata")); return result; }
                        auto values=vtkSmartPointer<vtkDoubleArray>::New(); values->SetName(name); values->SetNumberOfTuples(static_cast<vtkIdType>(upper[0]));
                        if(cg_field_read(file.id,b,z,sol,name,CGNS_ENUMV(RealDouble),lo,upper,values->GetPointer(0))!=CG_OK) { result.error=reason(QStringLiteral("CGNS field")); return result; }
                        FieldInfo info; info.name=QString::fromLocal8Bit(name); info.association=point?QStringLiteral("point"):QStringLiteral("cell");
                        const double *range=values->GetRange(); info.hasRange=std::isfinite(range[0]) && std::isfinite(range[1]) && range[0]<=range[1];
                        if(info.hasRange) { info.minimum=range[0]; info.maximum=range[1]; }
                        region.fields.append(info);
                        if(point) {
                            volume->GetPointData()->AddArray(values);
                            for(int si=0;si<surfaces.size();++si) {
                                auto boundaryValues=vtkSmartPointer<vtkDoubleArray>::New(); boundaryValues->SetName(name);
                                boundaryValues->SetNumberOfTuples(surfacePointIds[si].size());
                                for(int pi=0;pi<surfacePointIds[si].size();++pi) boundaryValues->SetValue(pi,values->GetValue(static_cast<vtkIdType>(surfacePointIds[si][pi])));
                                surfaces[si].data->GetPointData()->AddArray(boundaryValues);
                                FieldInfo boundaryInfo=info; const double *boundaryRange=boundaryValues->GetRange();
                                boundaryInfo.hasRange=std::isfinite(boundaryRange[0]) && std::isfinite(boundaryRange[1]) && boundaryRange[0]<=boundaryRange[1];
                                if(boundaryInfo.hasRange) { boundaryInfo.minimum=boundaryRange[0]; boundaryInfo.maximum=boundaryRange[1]; }
                                surfaces[si].fields.append(boundaryInfo);
                            }
                        }
                        else volume->GetCellData()->AddArray(values);
                    }
                }
                result.dataset.regions.append(region); for(const auto &surface:surfaces) result.dataset.regions.append(surface);
            }
        }
        if(result.dataset.regions.isEmpty()) result.error=QStringLiteral("CGNS contains no readable regions");
        return result;
    }
};
}
std::unique_ptr<DataAdapter> createCgnsAdapter() { return std::unique_ptr<DataAdapter>(new CgnsAdapter); }
} }
