#include "aerosim/visualization/DataAdapter.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cgnslib.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkPointData.h>
#include <iostream>
#include <cmath>
#define CHECK(x) do { if (!(x)) { std::cerr << "check failed: " << #x << " line " << __LINE__ << "\n"; return 1; } } while(false)
#define CG(x) do { if ((x) != CG_OK) { std::cerr << cg_get_error() << " line " << __LINE__ << "\n"; return 1; } } while(false)
int main(int argc, char **argv) {
    QCoreApplication app(argc,argv); qputenv("AEROSIM_CGNS_BACKEND", "paraview");
    if (argc == 2) {
        const auto result = aerosim::visualization::createCgnsAdapter()->read(QString::fromLocal8Bit(argv[1]));
        if (!result.ok()) { std::cerr << result.error.toStdString() << "\n"; return 2; }
        for (const auto &region : result.dataset.regions)
            std::cout << region.name.toStdString() << " kind=" << region.kind.toStdString() << " cells=" << region.data->GetNumberOfCells() << " fields=" << region.fields.size() << "\n";
        return 0;
    }
    QTemporaryDir dir; CHECK(dir.isValid());
    const QString path=dir.filePath(QStringLiteral("test.cgns"));
    int f=0,b=0,z=0,c=0,s=0,a=0,bc=0,section=0;
    CG(cg_open(QFile::encodeName(path).constData(),CG_MODE_WRITE,&f));
    CG(cg_base_write(f,"Base",3,3,&b)); cgsize_t sizes[3]={5,2,0};
    CG(cg_zone_write(f,b,"Fluid",sizes,CGNS_ENUMV(Unstructured),&z));
    const double x[5]={0,1,0,0,1}, y[5]={0,0,1,0,1}, zz[5]={0,0,0,1,1};
    CG(cg_coord_write(f,b,z,CGNS_ENUMV(RealDouble),"CoordinateX",x,&c));
    CG(cg_coord_write(f,b,z,CGNS_ENUMV(RealDouble),"CoordinateY",y,&c));
    CG(cg_coord_write(f,b,z,CGNS_ENUMV(RealDouble),"CoordinateZ",zz,&c));
    const cgsize_t volume[8]={1,2,3,4,2,3,4,5}, faces[6]={1,2,3,2,3,5};
    CG(cg_section_write(f,b,z,"Volume",CGNS_ENUMV(TETRA_4),1,2,0,volume,&section));
    CG(cg_section_write(f,b,z,"Surfaces",CGNS_ENUMV(TRI_3),3,4,0,faces,&section));
    const cgsize_t first[1]={3}, second[1]={4};
    CG(cg_boco_write(f,b,z,"Inlet",CGNS_ENUMV(BCInflow),CGNS_ENUMV(PointList),1,first,&bc));
    CG(cg_boco_gridlocation_write(f,b,z,bc,CGNS_ENUMV(FaceCenter)));
    CG(cg_boco_write(f,b,z,"Outlet",CGNS_ENUMV(BCOutflow),CGNS_ENUMV(PointList),1,second,&bc));
    CG(cg_boco_gridlocation_write(f,b,z,bc,CGNS_ENUMV(FaceCenter)));
    const double values[2]={12.5,83.0}; CG(cg_sol_write(f,b,z,"Solution",CGNS_ENUMV(CellCenter),&s));
    CG(cg_field_write(f,b,z,s,CGNS_ENUMV(RealDouble),"CustomField",values,&a)); CG(cg_close(f));
    auto adapter=aerosim::visualization::createCgnsAdapter();
    const auto result=adapter->read(path); if(!result.ok()) std::cerr<<result.error.toStdString()<<"\n";
    CHECK(result.ok() && result.dataset.regions.size()==3);
    const auto &volumeRegion=result.dataset.regions[0];
    CHECK(volumeRegion.dimension==3 && volumeRegion.data->GetNumberOfPoints()==5 && volumeRegion.data->GetNumberOfCells()==2);
    auto *field=volumeRegion.data->GetCellData()->GetArray("CustomField");
    CHECK(field && field->GetNumberOfTuples()==2 && field->GetComponent(0,0)==12.5 && field->GetComponent(1,0)==83.0);
    bool inlet=false,outlet=false;
    for(const auto &region:result.dataset.regions) {
        if(region.name.endsWith("Inlet")) { inlet=true; CHECK(region.data->GetNumberOfCells()==1 && region.data->GetNumberOfPoints()==3); }
        if(region.name.endsWith("Outlet")) { outlet=true; CHECK(region.data->GetNumberOfCells()==1 && region.data->GetNumberOfPoints()==3); }
    }
    CHECK(inlet && outlet);
    // A BC element ID pointing at a volume cell must not be silently shown
    // as an unclassified surface.
    CG(cg_open(QFile::encodeName(path).constData(),CG_MODE_MODIFY,&f));
    CG(cg_goto(f,b,"Zone_t",z,"ZoneBC_t",1,"end"));
    CG(cg_delete_node("Inlet"));
    const cgsize_t invalidFace[1]={1};
    CG(cg_boco_write(f,b,z,"InvalidBoundary",CGNS_ENUMV(BCWall),CGNS_ENUMV(PointList),1,invalidFace,&bc));
    CG(cg_boco_gridlocation_write(f,b,z,bc,CGNS_ENUMV(FaceCenter)));
    CG(cg_close(f));
    const auto invalid=adapter->read(path);
    CHECK(!invalid.ok() && invalid.error.contains(QStringLiteral("InvalidBoundary")));
    CHECK(!adapter->read(dir.filePath(QStringLiteral("missing.cgns"))).ok());
    const QString structured=dir.filePath(QStringLiteral("structured.cgns"));
    CG(cg_open(QFile::encodeName(structured).constData(),CG_MODE_WRITE,&f));
    CG(cg_base_write(f,"StructuredBase",3,3,&b)); cgsize_t dims[9]={2,2,2,1,1,1,0,0,0};
    CG(cg_zone_write(f,b,"StructuredZone",dims,CGNS_ENUMV(Structured),&z));
    const double sx[8]={0,1,0,1,0,1,0,1}, sy[8]={0,0,1,1,0,0,1,1}, sz[8]={0,0,0,0,1,1,1,1};
    CG(cg_coord_write(f,b,z,CGNS_ENUMV(RealDouble),"CoordinateX",sx,&c));
    CG(cg_coord_write(f,b,z,CGNS_ENUMV(RealDouble),"CoordinateY",sy,&c));
    CG(cg_coord_write(f,b,z,CGNS_ENUMV(RealDouble),"CoordinateZ",sz,&c));
    const cgsize_t faceRange[6]={1,1,1,1,2,2};
    CG(cg_boco_write(f,b,z,"Wall",CGNS_ENUMV(BCWall),CGNS_ENUMV(PointRange),2,faceRange,&bc));
    const double firstValue[1]={21.0}, secondValue[1]={42.0};
    CG(cg_sol_write(f,b,z,"Solution",CGNS_ENUMV(CellCenter),&s));
    CG(cg_field_write(f,b,z,s,CGNS_ENUMV(RealDouble),"FirstZoneField",firstValue,&a));
    CG(cg_zone_write(f,b,"SecondZone",dims,CGNS_ENUMV(Structured),&z));
    CG(cg_coord_write(f,b,z,CGNS_ENUMV(RealDouble),"CoordinateX",sx,&c));
    CG(cg_coord_write(f,b,z,CGNS_ENUMV(RealDouble),"CoordinateY",sy,&c));
    CG(cg_coord_write(f,b,z,CGNS_ENUMV(RealDouble),"CoordinateZ",sz,&c));
    CG(cg_sol_write(f,b,z,"Solution",CGNS_ENUMV(CellCenter),&s));
    CG(cg_field_write(f,b,z,s,CGNS_ENUMV(RealDouble),"SecondZoneField",secondValue,&a));
    CG(cg_close(f));
    const auto sr=adapter->read(structured); if(!sr.ok()) std::cerr<<sr.error.toStdString()<<"\n";
    CHECK(sr.ok() && sr.dataset.regions.size()==3);
    bool wall=false;
    for (const auto &region : sr.dataset.regions)
        if (region.name.endsWith("Wall")) wall=region.kind==QStringLiteral("boundary") && region.data->GetNumberOfCells()==1;
    CHECK(wall);
    bool firstField=false, secondField=false;
    for (const auto &region : sr.dataset.regions) {
        if (region.name.contains("StructuredZone")) { auto *v=region.data->GetCellData()->GetArray("FirstZoneField"); firstField=v && v->GetTuple1(0)==21.0; }
        if (region.name.contains("SecondZone")) { auto *v=region.data->GetCellData()->GetArray("SecondZoneField"); secondField=v && v->GetTuple1(0)==42.0; }
    }
    CHECK(firstField && secondField);
    CG(cg_open(QFile::encodeName(structured).constData(),CG_MODE_MODIFY,&f));
    int targetZone=0, zoneCount=0; CG(cg_nzones(f,b,&zoneCount));
    for (int i=1;i<=zoneCount;++i) { char zoneName[33]={}; cgsize_t zoneSize[9]={}; CG(cg_zone_read(f,b,i,zoneName,zoneSize)); if (QString::fromLatin1(zoneName)==QStringLiteral("StructuredZone")) targetZone=i; }
    CHECK(targetZone>0);
    CG(cg_goto(f,b,"Zone_t",targetZone,"ZoneBC_t",1,"end"));
    CG(cg_delete_node("Wall"));
    const cgsize_t outsideRange[6]={1,1,1,1,3,2};
    CG(cg_boco_write(f,b,targetZone,"Outside",CGNS_ENUMV(BCWall),CGNS_ENUMV(PointRange),2,outsideRange,&bc));
    CG(cg_close(f));
    const auto badStructured=adapter->read(structured);
    CHECK(!badStructured.ok() && badStructured.error.contains(QStringLiteral("PointRange")));
    std::cout<<"ParaView CGNS adapter: boundaries, fields, structured and missing-file checks passed\n";
    return 0;
}
