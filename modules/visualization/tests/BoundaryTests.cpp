#include "aerosim/visualization/features/BoundarySurfaceExtractor.h"
#include "aerosim/visualization/data/BoundaryMeshReader.h"
#include "aerosim/visualization/data/BoundaryLoadService.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDoubleArray.h>
#include <vtkIdTypeArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkUnstructuredGrid.h>
#include <vtkUnsignedCharArray.h>
#include <vtkXMLImageDataWriter.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkXMLUnstructuredGridWriter.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <set>
using namespace aerosim::visualization;
static void require(bool ok,const char* what) {if(!ok) throw std::runtime_error(what);}
template<class Fn> static void rejects(Fn fn) {bool failed=false;try{fn();}catch(const std::exception&){failed=true;}require(failed,"invalid input accepted");}
static vtkSmartPointer<vtkUnstructuredGrid> pairOfTetra() {
    vtkNew<vtkPoints> points;
    points->InsertNextPoint(0,0,0); points->InsertNextPoint(1,0,0); points->InsertNextPoint(0,1,0);
    points->InsertNextPoint(0,0,1); points->InsertNextPoint(0,0,-1);
    auto grid=vtkSmartPointer<vtkUnstructuredGrid>::New(); grid->SetPoints(points);
    vtkIdType a[]={0,1,2,3},b[]={0,2,1,4}; grid->InsertNextCell(VTK_TETRA,4,a);grid->InsertNextCell(VTK_TETRA,4,b);
    vtkNew<vtkDoubleArray> zones; zones->SetName("ZoneId");zones->InsertNextValue(10);zones->InsertNextValue(20);
    grid->GetCellData()->AddArray(zones); return grid;
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        const QString fixture=QString::fromUtf8(AEROSIM_FIXTURE_DIR)+"/cube_t00.vtk";
        auto cube=BoundaryMeshReader::read(fixture);
        require(cube->inputPoints==27 && cube->inputCells==8 && cube->boundaryFaces==24 && cube->uniqueBoundaryPoints==26,"cube counts");
        auto *source = vtkDataSet::SafeDownCast(cube->source.GetPointer());
        require(source && source->GetNumberOfCells()==8,"source lost");
        auto* original=vtkIdTypeArray::SafeDownCast(cube->surface->GetPointData()->GetArray("vtkOriginalPointIds"));
        for(const char* field:{"Pressure","Temperature","Phi","Velocity"}) {
            auto* a=cube->surface->GetPointData()->GetArray(field); auto* b=source->GetPointData()->GetArray(field);
            require(a&&b,"missing field");
            for(vtkIdType i=0;i<a->GetNumberOfTuples();++i)
                for(int c=0;c<a->GetNumberOfComponents();++c)
                    require(a->GetComponent(i,c)==b->GetComponent(original->GetValue(i),c),"point field mapping");
        }
        double bounds[6];cube->surface->GetBounds(bounds);
        for(int i=0;i<6;++i) require(bounds[i]==(i%2?1:-1),"cube bounds");
        double area=0;
        for(vtkIdType i=0;i<cube->surface->GetNumberOfCells();++i) {
            auto* cell=cube->surface->GetCell(i); double p[3],q[3],r[3];
            cell->GetPoints()->GetPoint(0,p);cell->GetPoints()->GetPoint(1,q);cell->GetPoints()->GetPoint(3,r);
            double u[3],v[3];for(int j=0;j<3;++j){u[j]=q[j]-p[j];v[j]=r[j]-p[j];}
            double cross[3]={u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};
            area+=std::sqrt(cross[0]*cross[0]+cross[1]*cross[1]+cross[2]*cross[2]);
            require(cross[0]*(p[0]+q[0]+r[0])+cross[1]*(p[1]+q[1]+r[1])+cross[2]*(p[2]+q[2]+r[2])>0,"cube outward winding");
        }
        require(area==24,"cube area");
        auto grid=pairOfTetra();auto tetra=BoundarySurfaceExtractor::extract(grid);
        require(tetra->boundaryFaces==6,"shared tetra face retained");
        auto* owner=vtkIdTypeArray::SafeDownCast(tetra->surface->GetCellData()->GetArray("vtkOriginalCellIds"));
        auto* zone=tetra->surface->GetCellData()->GetArray("ZoneId");
        for(vtkIdType i=0;i<6;++i) require(zone->GetTuple1(i)==(owner->GetValue(i)==0?10:20),"cell data mapping");
        vtkNew<vtkImageData> image;image->SetDimensions(3,2,2);
        vtkNew<vtkUnstructuredGrid> voxels;vtkNew<vtkPoints> points;
        for(vtkIdType i=0;i<image->GetNumberOfPoints();++i) points->InsertNextPoint(image->GetPoint(i));
        voxels->SetPoints(points);
        for(vtkIdType i=0;i<image->GetNumberOfCells();++i) voxels->InsertNextCell(VTK_VOXEL,image->GetCell(i)->GetPointIds());
        require(BoundarySurfaceExtractor::extract(voxels)->boundaryFaces==10,"adjacent voxel faces");
        image->SetDimensions(2,2,2); require(BoundarySurfaceExtractor::extract(image)->boundaryFaces==6,"single cell");
        image->SetDimensions(101,101,101); auto large=BoundarySurfaceExtractor::extract(image);
        require(large->inputCells==1000000 && large->boundaryFaces==60000 && large->uniqueBoundaryPoints==60002,"large structured grid counts");
        std::atomic_bool cancel{true}; rejects([&]{BoundarySurfaceExtractor::extract(grid,&cancel);});
        rejects([]{BoundarySurfaceExtractor::extract(nullptr);});
        auto duplicate=pairOfTetra();duplicate->GetCellData()->Initialize();
        vtkIdType dup[]={0,1,2,3};duplicate->InsertNextCell(VTK_TETRA,4,dup);
        rejects([&]{BoundarySurfaceExtractor::extract(duplicate);});
        auto bad=pairOfTetra();bad->GetCellData()->Initialize();vtkIdType wrong[]={0,1,2,999};bad->InsertNextCell(VTK_TETRA,4,wrong);
        rejects([&]{BoundarySurfaceExtractor::extract(bad);});
        auto high=pairOfTetra();high->GetCellData()->Initialize();high->InsertNextCell(VTK_QUADRATIC_TETRA,4,dup);
        rejects([&]{BoundarySurfaceExtractor::extract(high);});
        auto nonmanifold=pairOfTetra();nonmanifold->GetCellData()->Initialize();
        nonmanifold->GetPoints()->InsertNextPoint(0,0,2);
        vtkIdType third[]={0,1,2,5};nonmanifold->InsertNextCell(VTK_TETRA,4,third);
        rejects([&]{BoundarySurfaceExtractor::extract(nonmanifold);});
        auto ghost=pairOfTetra();vtkNew<vtkUnsignedCharArray> flags;
        flags->SetName("vtkGhostType");flags->InsertNextValue(1);flags->InsertNextValue(0);
        ghost->GetCellData()->AddArray(flags);
        rejects([&]{BoundarySurfaceExtractor::extract(ghost);});
        // Check each supported linear volume topology, not only voxel fixtures.
        for(const auto type:{VTK_HEXAHEDRON,VTK_WEDGE,VTK_PYRAMID}) {
            vtkNew<vtkUnstructuredGrid> single;vtkNew<vtkPoints> nodes;
            const double xyz[][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
            for(const auto& p:xyz) nodes->InsertNextPoint(p);
            single->SetPoints(nodes);vtkIdType all[]={0,1,2,3,4,5,6,7};
            vtkIdType wedge[]={0,1,3,4,5,7},pyramid[]={0,1,2,3,4};
            single->InsertNextCell(type,type==VTK_HEXAHEDRON?8:(type==VTK_WEDGE?6:5),
                type==VTK_HEXAHEDRON?all:(type==VTK_WEDGE?wedge:pyramid));
            require(BoundarySurfaceExtractor::extract(single)->boundaryFaces==(type==VTK_HEXAHEDRON?6:5),"linear cell topology");
        }
        QTemporaryDir temp;require(temp.isValid(),"temporary directory");
        auto xml=temp.filePath("pair.vtu");vtkNew<vtkXMLUnstructuredGridWriter> writer;
        writer->SetFileName(QFile::encodeName(xml).constData());writer->SetInputData(grid);require(writer->Write()==1,"fixture write");
        require(BoundaryMeshReader::read(xml)->boundaryFaces==6,"XML read");
        image->SetDimensions(3,2,2);auto vti=temp.filePath("volume.vti");
        vtkNew<vtkXMLImageDataWriter> imageWriter;imageWriter->SetInputData(image);
        imageWriter->SetFileName(QFile::encodeName(vti).constData());require(imageWriter->Write()==1,"image write");
        require(BoundaryMeshReader::read(vti)->boundaryFaces==10,"VTI read");
        vtkNew<vtkPolyData> poly;poly->DeepCopy(cube->surface);
        poly->GetPointData()->RemoveArray("vtkOriginalPointIds");poly->GetCellData()->RemoveArray("vtkOriginalCellIds");
        auto vtp=temp.filePath("surface.vtp");vtkNew<vtkXMLPolyDataWriter> polyWriter;
        polyWriter->SetInputData(poly);polyWriter->SetFileName(QFile::encodeName(vtp).constData());
        require(polyWriter->Write()==1,"surface write");
        auto surface=BoundaryMeshReader::read(vtp);require(!surface->volume && surface->boundaryFaces==24,"VTP surface preserved");
        auto broken=temp.filePath("broken.vtk");QFile f(broken);require(f.open(QIODevice::WriteOnly),"bad file open");f.write("not a mesh");f.close();
        rejects([&]{BoundaryMeshReader::read(broken);});
        BoundaryLoadService service;QEventLoop loop;bool loaded=false;QString id;
        QObject::connect(&service,&BoundaryLoadService::loaded,&app,[&](const QString& value,const QString&,const QString&){
            id=value;loaded=service.mesh(id)->boundaryFaces==24 && QThread::currentThread()==app.thread();loop.quit();
        });
        QTimer::singleShot(10000,&loop,&QEventLoop::quit);service.load(fixture);loop.exec();
        require(loaded && !service.busy(),"async handoff");service.remove(id);require(!service.mesh(id),"resource release");
        QObject::disconnect(&service,nullptr,&app,nullptr);bool cancelled=false;loaded=false;
        QObject::connect(&service,&BoundaryLoadService::started,&app,[&](const QString& value,const QString&){id=value;service.remove(id);});
        QObject::connect(&service,&BoundaryLoadService::loaded,&app,[&]{loaded=true;loop.quit();});
        QObject::connect(&service,&BoundaryLoadService::failed,&app,[&]{cancelled=true;loop.quit();});
        QTimer::singleShot(10000,&loop,&QEventLoop::quit);service.load(fixture);loop.exec();
        require(cancelled&&!loaded&&!service.mesh(id),"cancelled result resurrected");
        QObject::disconnect(&service,nullptr,&app,nullptr);
        QStringList completed;
        QStringList failures;
        QStringList ids;
        int idleCount = 0;
        QObject::connect(&service, &BoundaryLoadService::loaded, &app,
            [&](const QString &value, const QString &path, const QString &) {
                ids.append(value);
                completed.append(path);
            });
        QObject::connect(&service, &BoundaryLoadService::failed, &app,
            [&](const QString &, const QString &message) { failures.append(message); });
        QObject::connect(&service, &BoundaryLoadService::busyChanged, &app, [&](bool busy) {
            if (!busy) {
                ++idleCount;
                loop.quit();
            }
        });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        service.loadFiles({fixture, broken, vtp});
        loop.exec();
        require(completed == QStringList{fixture, vtp}, "mixed batch must continue after a broken file");
        require(failures.size() == 1 && failures.first().contains("broken.vtk"), "batch failure needs its file name");
        require(ids.size() == 2 && ids[0] != ids[1] && service.mesh(ids[0]) && service.mesh(ids[1]),
                "each imported file must keep its own data and ID");
        require(idleCount == 1 && !service.busy(), "batch must return to idle once");
        std::cout<<"Boundary tests passed: geometry, fields, IDs, winding, large grid, readers, rejection, async, cancellation\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
    return 0;
}
