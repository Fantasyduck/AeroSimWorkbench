#include "aerosim/visualization/features/BoundarySurfaceExtractor.h"
#include <vtkCell.h>
#include <vtkAlgorithm.h>
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkIdTypeArray.h>
#include <vtkIdList.h>
#include <vtkUnstructuredGrid.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <vector>
#include <stdexcept>
#include <unordered_map>
#ifdef AEROSIM_VTK_DEBUG_ONLY
#error VTK integration requires x64 Debug.
#endif
namespace aerosim { namespace visualization {
namespace {
void checkCancel(const std::atomic_bool* flag) {
    if(flag && flag->load()) throw std::runtime_error("已取消导入");
}
struct FaceKey {
    std::array<vtkIdType,4> ids{{-1,-1,-1,-1}};
    bool operator==(const FaceKey& b) const { return ids==b.ids; }
};
struct FaceHash {
    size_t operator()(const FaceKey& key) const {
        size_t h=0;
        for(auto id:key.ids) h^=std::hash<vtkIdType>{}(id)+0x9e3779b9+(h<<6)+(h>>2);
        return h;
    }
};
}
QString BoundaryMesh::summary() const {
    return QString::fromUtf8("输入 %1 点 / %2 %3 → 边界 %4 面 / %5 唯一节点（%6 点条目）")
        .arg(inputPoints).arg(inputCells).arg(volume?QString::fromUtf8("体单元"):QString::fromUtf8("表面单元"))
        .arg(boundaryFaces).arg(uniqueBoundaryPoints).arg(surface->GetNumberOfPoints());
}
std::shared_ptr<const BoundaryMesh> BoundarySurfaceExtractor::extract(vtkDataSet* input,
    const std::atomic_bool* cancelled) {
    checkCancel(cancelled);
    if(!input || !input->GetNumberOfPoints() || !input->GetNumberOfCells()) throw std::runtime_error("网格为空");
    auto* image=vtkImageData::SafeDownCast(input);
    auto* grid=vtkUnstructuredGrid::SafeDownCast(input);
    auto* poly=vtkPolyData::SafeDownCast(input);
    if(!image && !grid && !poly) throw std::runtime_error("仅支持三维 ImageData、线性非结构网格及表面 PolyData");
    if(input->CheckAttributes()) throw std::runtime_error("字段长度与网格不一致");
    for(auto* attrs:{static_cast<vtkDataSetAttributes*>(input->GetPointData()),static_cast<vtkDataSetAttributes*>(input->GetCellData())}) {
        if(attrs->HasArray("vtkOriginalPointIds") || attrs->HasArray("vtkOriginalCellIds")) throw std::runtime_error("输入含保留的来源 ID 数组名，请先重命名");
        if(auto* ghosts=attrs->GetArray("vtkGhostType"))
            for(vtkIdType i=0;i<ghosts->GetNumberOfTuples();++i)
                if(ghosts->GetComponent(i,0)!=0) throw std::runtime_error("首版不支持 ghost 或隐藏实体");
    }
    const auto np=input->GetNumberOfPoints();
    for(vtkIdType i=0;i<np;++i) {
        if((i&4095)==0) checkCancel(cancelled);
        double p[3]; input->GetPoint(i,p);
        for(double x:p) if(!std::isfinite(x)) throw std::runtime_error("坐标含非有限值");
    }
    if(image) {
        int dims[3]; image->GetDimensions(dims);
        for(int d:dims) if(d<2) throw std::runtime_error("ImageData 必须为三维体网格");
        for(int i=0;i<3;++i) if(!std::isfinite(image->GetSpacing()[i]) || image->GetSpacing()[i]<=0)
            throw std::runtime_error("间距必须为有限正数");
    } else {
        vtkNew<vtkIdList> ids;
        std::unordered_map<FaceKey,unsigned char,FaceHash> faces;
        std::set<std::vector<vtkIdType>> cells;
        for(vtkIdType i=0;i<input->GetNumberOfCells();++i) {
            if((i&4095)==0) checkCancel(cancelled);
            const int type=input->GetCellType(i);
            int expected=0;
            if(grid) {
                switch(type) {
                case VTK_TETRA: expected=4; break;
                case VTK_HEXAHEDRON: case VTK_VOXEL: expected=8; break;
                case VTK_WEDGE: expected=6; break;
                case VTK_PYRAMID: expected=5; break;
                default: throw std::runtime_error("不支持高阶、多面体或混合维度体网格");
                }
            } else {
                if(type==VTK_TRIANGLE) expected=3;
                else if(type==VTK_QUAD) expected=4;
                else throw std::runtime_error("表面仅支持三角形和四边形");
            }
            input->GetCellPoints(i,ids);
            if(ids->GetNumberOfIds()!=expected) throw std::runtime_error("单元节点数不合法");
            std::vector<vtkIdType> key;
            for(vtkIdType j=0;j<ids->GetNumberOfIds();++j) {
                auto id=ids->GetId(j);
                if(id<0 || id>=np) throw std::runtime_error("单元节点索引越界");
                key.push_back(id);
            }
            std::sort(key.begin(),key.end());
            if(std::adjacent_find(key.begin(),key.end())!=key.end()) throw std::runtime_error("单元含重复节点");
            if(!cells.insert(key).second) throw std::runtime_error("网格含重复单元");
            if(grid) {
                auto* cell=input->GetCell(i);
                for(int f=0;f<cell->GetNumberOfFaces();++f) {
                    auto* face=cell->GetFace(f); FaceKey fk;
                    for(int j=0;j<face->GetNumberOfPoints();++j) fk.ids[j]=face->GetPointId(j);
                    std::sort(fk.ids.begin(),fk.ids.end());
                    if(++faces[fk]>2) throw std::runtime_error("非流形面被超过两个体单元共享");
                }
            }
        }
    }
    checkCancel(cancelled);
    vtkNew<vtkDataSetSurfaceFilter> filter;
    struct ObserverState {const std::atomic_bool* cancel; bool error=false;} observation{cancelled};
    vtkNew<vtkCallbackCommand> observer;
    observer->SetClientData(&observation);
    observer->SetCallback([](vtkObject* object,unsigned long event,void* client,void*) {
        auto* state=static_cast<ObserverState*>(client);
        if(event==vtkCommand::ErrorEvent) state->error=true;
        if(state->cancel && state->cancel->load()) vtkAlgorithm::SafeDownCast(object)->SetAbortExecute(1);
    });
    filter->AddObserver(vtkCommand::ErrorEvent,observer); filter->AddObserver(vtkCommand::ProgressEvent,observer);
    filter->SetInputData(input); filter->UseStripsOff();
    filter->PassThroughPointIdsOn(); filter->PassThroughCellIdsOn(); filter->Update();
    checkCancel(cancelled);
    if(observation.error) throw std::runtime_error("VTK 边界提取失败");
    auto r=std::make_shared<BoundaryMesh>();
    r->source=input; r->surface=vtkSmartPointer<vtkPolyData>::New();
    r->surface->ShallowCopy(filter->GetOutput());
    r->inputPoints=np; r->inputCells=input->GetNumberOfCells(); r->volume=!poly;
    r->boundaryFaces=r->surface->GetNumberOfPolys();
    auto* points=vtkIdTypeArray::SafeDownCast(r->surface->GetPointData()->GetArray("vtkOriginalPointIds"));
    auto* cellIds=vtkIdTypeArray::SafeDownCast(r->surface->GetCellData()->GetArray("vtkOriginalCellIds"));
    if(!r->boundaryFaces || !points || !cellIds || points->GetNumberOfTuples()!=r->surface->GetNumberOfPoints()
        || cellIds->GetNumberOfTuples()!=r->surface->GetNumberOfCells()) throw std::runtime_error("未生成有效边界或来源映射");
    std::set<vtkIdType> unique;
    for(vtkIdType i=0;i<points->GetNumberOfTuples();++i) unique.insert(points->GetValue(i));
    r->uniqueBoundaryPoints=static_cast<vtkIdType>(unique.size());
    return r;
}
} }
