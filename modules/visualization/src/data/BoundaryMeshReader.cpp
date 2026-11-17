#include "aerosim/visualization/data/BoundaryMeshReader.h"
#include <QFile>
#include <QFileInfo>
#include <vtkCallbackCommand.h>
#include <vtkAlgorithm.h>
#include <vtkCommand.h>
#include <vtkDataSetReader.h>
#include <vtkNew.h>
#include <vtkXMLGenericDataObjectReader.h>
#include <stdexcept>
#ifdef AEROSIM_MESH_HAS_CGNS
#include "aerosim/visualization/DataAdapter.h"
#include <vtkCompositeDataGeometryFilter.h>
#include <vtkMultiBlockDataSet.h>
#endif
namespace aerosim { namespace visualization {
std::shared_ptr<const BoundaryMesh> BoundaryMeshReader::read(const QString& path,const std::atomic_bool* cancelled) {
    if(!QFileInfo(path).isFile()) throw std::runtime_error("网格文件不存在");
    auto suffix=QFileInfo(path).suffix().toLower();
#ifdef AEROSIM_MESH_HAS_CGNS
    if (suffix == QStringLiteral("cgns")) {
        const auto result = createCgnsAdapter()->read(path);
        if (!result.ok()) throw std::runtime_error(result.error.toUtf8().constData());
        if (cancelled && cancelled->load()) throw std::runtime_error("已取消导入");

        vtkNew<vtkMultiBlockDataSet> blocks;
        unsigned int index = 0;
        qint64 points = 0;
        qint64 cells = 0;
        bool volume = false;
        for (const auto &region : result.dataset.regions) {
            if (!region.data || region.dimension < 2) continue;
            blocks->SetBlock(index++, region.data);
            points += region.data->GetNumberOfPoints();
            cells += region.data->GetNumberOfCells();
            volume = volume || region.dimension == 3;
        }
        if (index == 0) throw std::runtime_error("CGNS 文件没有可显示的面或体网格");
        vtkNew<vtkCompositeDataGeometryFilter> geometry;
        geometry->SetInputDataObject(blocks);
        geometry->Update();
        if (cancelled && cancelled->load()) throw std::runtime_error("已取消导入");
        auto mesh = std::make_shared<BoundaryMesh>();
        mesh->source = blocks.GetPointer();
        mesh->surface = geometry->GetOutput();
        mesh->boundaryFaces = mesh->surface->GetNumberOfPolys();
        if (!mesh->boundaryFaces) throw std::runtime_error("CGNS 文件没有生成可显示的边界");
        mesh->inputPoints = points;
        mesh->inputCells = cells;
        mesh->uniqueBoundaryPoints = mesh->surface->GetNumberOfPoints();
        mesh->volume = volume;
        return mesh;
    }
#endif
    struct State {const std::atomic_bool* cancel;bool error=false;} state{cancelled};
    vtkNew<vtkCallbackCommand> observer;
    observer->SetClientData(&state);
    observer->SetCallback([](vtkObject* object,unsigned long event,void* client,void*) {
        auto* s=static_cast<State*>(client);
        if(event==vtkCommand::ErrorEvent) s->error=true;
        if(s->cancel && s->cancel->load()) vtkAlgorithm::SafeDownCast(object)->SetAbortExecute(1);
    });
    vtkSmartPointer<vtkDataSet> data;
    const auto bytes=QFile::encodeName(path);
    if(QFile::decodeName(bytes)!=path) throw std::runtime_error("VTK 8.2 无法表示此文件路径");
    if(suffix=="vtk") {
        vtkNew<vtkDataSetReader> reader; reader->AddObserver(vtkCommand::ErrorEvent,observer);
        reader->AddObserver(vtkCommand::ProgressEvent,observer);
        reader->SetFileName(bytes.constData()); reader->ReadAllScalarsOn(); reader->ReadAllVectorsOn(); reader->ReadAllFieldsOn();
        reader->Update(); data=reader->GetOutput();
    } else if(suffix=="vtu" || suffix=="vtp" || suffix=="vti") {
        vtkNew<vtkXMLGenericDataObjectReader> reader; reader->AddObserver(vtkCommand::ErrorEvent,observer);
        reader->AddObserver(vtkCommand::ProgressEvent,observer);
        reader->SetFileName(bytes.constData()); reader->Update(); data=vtkDataSet::SafeDownCast(reader->GetOutput());
    } else throw std::runtime_error("支持的格式为 .vtk、.vtu、.vtp、.vti");
    if(state.error || !data) throw std::runtime_error("VTK 读取失败：文件损坏、类型不支持或路径编码不兼容");
    return BoundarySurfaceExtractor::extract(data,cancelled);
}
} }
