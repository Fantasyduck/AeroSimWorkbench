#if defined(_MSC_VER) && !defined(_DEBUG)
#error The current CGNS/VTK integration is verified for Windows Debug only.
#endif
#include "aerosim/visualization/VtkSceneViewer.h"
#include "aerosim/visualization/DataAdapter.h"
#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include <QColor>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QPointer>
#include <QUuid>
#include <QVBoxLayout>
#include <QMap>
#include <QLabel>
#include <QFrame>
#include <QPixmap>
#include <QPainter>
#include <QResizeEvent>
#include <QtConcurrent/QtConcurrentRun>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCell.h>
#include <vtkDataSet.h>
#include <vtkDataArray.h>
#include <vtkDataSetMapper.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkCellData.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkScalarsToColors.h>
#include <vtkLookupTable.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkAutoInit.h>
#include <vector>
#include <cmath>
VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);

namespace aerosim { namespace visualization {
class WorkbenchInteractorStyle : public vtkInteractorStyleTrackballCamera {
public:
    static WorkbenchInteractorStyle *New();
    vtkTypeMacro(WorkbenchInteractorStyle, vtkInteractorStyleTrackballCamera);
    QString mode = QStringLiteral("select");
    QString dragMode;
    void OnLeftButtonDown() override {
        dragMode = mode;
        if (dragMode == QLatin1String("pan")) Superclass::OnMiddleButtonDown();
        else if (dragMode == QLatin1String("zoom")) Superclass::OnRightButtonDown();
        else if (dragMode == QLatin1String("rotate")) Superclass::OnLeftButtonDown();
    }
    void OnLeftButtonUp() override {
        if (dragMode == QLatin1String("pan")) Superclass::OnMiddleButtonUp();
        else if (dragMode == QLatin1String("zoom")) Superclass::OnRightButtonUp();
        else if (dragMode == QLatin1String("rotate")) Superclass::OnLeftButtonUp();
        dragMode.clear();
    }
};
vtkStandardNewMacro(WorkbenchInteractorStyle);
struct LoadedDataset { QString id; ImportedData data; };
struct BatchResult { QVector<ImportedData> datasets; QString error; };
class VtkSceneViewer::Impl {
public:
    explicit Impl(VtkSceneViewer *owner) : owner(owner) {
        auto *layout=new QVBoxLayout(owner); layout->setContentsMargins(0,0,0,0);
        widget=new QVTKOpenGLNativeWidget(owner); layout->addWidget(widget);
        window=vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        renderer=vtkSmartPointer<vtkRenderer>::New(); window->AddRenderer(renderer); widget->SetRenderWindow(window);
        style=vtkSmartPointer<WorkbenchInteractorStyle>::New(); window->GetInteractor()->SetInteractorStyle(style);
        renderer->SetBackground(1,1,1); renderer->SetBackground2(.32,.34,.43); renderer->GradientBackgroundOn();
        legend=new QFrame(owner); legend->setAttribute(Qt::WA_TransparentForMouseEvents);
        legend->setStyleSheet(QStringLiteral("QFrame { background:rgba(255,255,255,220); } QLabel { background:transparent; color:#243447; }"));
        auto *legendLayout=new QVBoxLayout(legend); legendLayout->setContentsMargins(6,4,6,4); legendLayout->setSpacing(2);
        legendName=new QLabel(legend); legendMax=new QLabel(legend); legendColors=new QLabel(legend); legendMin=new QLabel(legend);
        legendLayout->addWidget(legendName); legendLayout->addWidget(legendMax); legendLayout->addWidget(legendColors); legendLayout->addWidget(legendMin); legend->hide();
    }
    void render() { window->Render(); }
    struct Display { QString dataset; QString region; vtkSmartPointer<vtkActor> actor; vtkSmartPointer<vtkDataSetMapper> mapper; bool active = false; bool customRepresentation = false; QString field, association; QHash<QString, QPair<double,double>> ranges; };
    void addDataset(const LoadedDataset &dataset) {
        for(const auto &region:dataset.data.regions) {
            Display display; display.dataset=dataset.id; display.region=region.id;
            display.mapper=vtkSmartPointer<vtkDataSetMapper>::New(); display.mapper->SetInputData(region.data); display.mapper->ScalarVisibilityOff();
            display.actor=vtkSmartPointer<vtkActor>::New(); display.actor->SetMapper(display.mapper); display.actor->GetProperty()->SetColor(.72,.78,.84);
            display.active=true; display.actor->SetVisibility(visibility.value(dataset.id,true)); renderer->AddActor(display.actor); displays.append(display);
        }
    }
    // Pick a useful view only when a surface is nearly edge-on. Surface area
    // projected along each direction also handles thin, curved boundary patches.
    bool orientEdgeOnRegion(vtkDataSet *data) {
        auto *camera=renderer->GetActiveCamera(); double direction[3]; camera->GetDirectionOfProjection(direction);
        double projected[3]={0,0,0}, current=0;
        for(vtkIdType ci=0;ci<data->GetNumberOfCells();++ci) {
            vtkCell *cell=data->GetCell(ci); if(cell->GetCellDimension()!=2 || cell->GetNumberOfPoints()<3) continue;
            double p0[3]; cell->GetPoints()->GetPoint(0,p0);
            for(vtkIdType i=1;i+1<cell->GetNumberOfPoints();++i) {
                double p1[3],p2[3]; cell->GetPoints()->GetPoint(i,p1); cell->GetPoints()->GetPoint(i+1,p2);
                const double a[3]={p1[0]-p0[0],p1[1]-p0[1],p1[2]-p0[2]};
                const double b[3]={p2[0]-p0[0],p2[1]-p0[1],p2[2]-p0[2]};
                const double n[3]={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
                for(int k=0;k<3;++k) projected[k]+=std::abs(n[k]);
                current+=std::abs(n[0]*direction[0]+n[1]*direction[1]+n[2]*direction[2]);
            }
        }
        int axis=0; if(projected[1]>projected[axis]) axis=1; if(projected[2]>projected[axis]) axis=2;
        if(projected[axis]<=0) return false;
        const double *bounds=data->GetBounds();
        const double spans[3]={bounds[1]-bounds[0],bounds[3]-bounds[2],bounds[5]-bounds[4]};
        const double longest=std::max(spans[0],std::max(spans[1],spans[2]));
        const double shortest=std::min(spans[0],std::min(spans[1],spans[2]));
        const bool thin=longest>0 && shortest>longest*1e-10 && shortest<longest*0.002;
        if(current>=projected[axis]*0.25) return thin;
        double focal[3]; camera->GetFocalPoint(focal); double position[3]={focal[0],focal[1],focal[2]};
        const double distance=camera->GetDistance()>0 ? camera->GetDistance() : 1.0;
        position[axis]+=direction[axis]>0 ? -distance : distance; camera->SetPosition(position);
        if(axis==1) camera->SetViewUp(0,0,1); else camera->SetViewUp(0,1,0);
        camera->OrthogonalizeViewUp();
        return thin;
    }
    void select(const QString &datasetId,const QString &regionId,const QString &field,const QString &association) {
        selectedDataset = datasetId; selectedRegion = regionId;
        if (field.isEmpty()) return;
        for(const auto &dataset:datasets) if(dataset.id==datasetId) for(const auto &region:dataset.data.regions) if(region.id==regionId) {
            for(auto &display:displays) if(display.dataset==datasetId && display.region==regionId) {
                const bool visible=display.actor->GetVisibility()!=0;
                if(!field.isEmpty()) {
                    const QByteArray name=field.toUtf8();
                    vtkDataArray *array=association=="cell" ? region.data->GetCellData()->GetArray(name.constData()) : region.data->GetPointData()->GetArray(name.constData());
                    if(array) {
                        display.field = field; display.association = association;
                        display.mapper->ScalarVisibilityOn(); display.mapper->SetScalarMode(association=="cell" ? VTK_SCALAR_MODE_USE_CELL_FIELD_DATA : VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
                        display.mapper->SelectColorArray(name.constData());
                        const QString rangeKey = association + QStringLiteral(":") + field;
                        double dataRange[2]; array->GetRange(dataRange, array->GetNumberOfComponents() == 1 ? 0 : -1);
                        const auto stored = display.ranges.value(rangeKey, qMakePair(dataRange[0], dataRange[1]));
                        const double range[] = {stored.first, stored.second};
                        display.mapper->SetScalarRange(range[0], range[1]);
                        auto lookup=vtkSmartPointer<vtkLookupTable>::New(); lookup->SetRange(range[0],range[1]); lookup->SetVectorModeToMagnitude();
                        lookup->SetHueRange(0.667,0.0); lookup->Build();
                        display.mapper->SetLookupTable(lookup); display.mapper->UseLookupTableScalarRangeOn();
                        QPixmap bar(18,128); QPainter painter(&bar);
                        for(int i=0;i<128;++i) {
                            double rgb[3]; lookup->GetColor(range[1]-(range[1]-range[0])*i/127.,rgb);
                            painter.setPen(QColor::fromRgbF(rgb[0],rgb[1],rgb[2])); painter.drawLine(0,i,17,i);
                        }
                        painter.end(); legendName->setText(field); legendMax->setText(QString::number(range[1],'g',6));
                        legendColors->setPixmap(bar); legendMin->setText(QString::number(range[0],'g',6));
                        legend->adjustSize(); legend->move(qMax(0,owner->width()-legend->width()-14),qMax(0,owner->height()-legend->height()-14));
                        legendDataset=datasetId; legendRegion=regionId;
                        if(visible) { legend->show(); legend->raise(); } else legend->hide();
                    } else { display.mapper->ScalarVisibilityOff(); legend->hide(); }
                } else { display.mapper->ScalarVisibilityOff(); legend->hide(); }
            } render(); return;
        }
    }
    VtkSceneViewer *owner; QVTKOpenGLNativeWidget *widget=nullptr;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> window;
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<WorkbenchInteractorStyle> style;
    QFrame *legend=nullptr;
    QString legendDataset, legendRegion;
    QString selectedDataset, selectedRegion;
    QLabel *legendName=nullptr; QLabel *legendMax=nullptr; QLabel *legendColors=nullptr; QLabel *legendMin=nullptr;
    QVector<LoadedDataset> datasets;
    QVector<Display> displays;
    QHash<QString,bool> visibility;
    bool busy=false;
};
VtkSceneViewer::VtkSceneViewer(QWidget *parent) : QWidget(parent), impl_(new Impl(this)) {}
VtkSceneViewer::~VtkSceneViewer() = default;
void VtkSceneViewer::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    impl_->legend->move(qMax(0,width()-impl_->legend->width()-14),qMax(0,height()-impl_->legend->height()-14));
}
void VtkSceneViewer::resetCamera() { impl_->renderer->ResetCamera(); impl_->render(); }
void VtkSceneViewer::setCameraOrientation(const QString &orientation) {
    auto *camera=impl_->renderer->GetActiveCamera(); double focal[3]; camera->GetFocalPoint(focal);
    const double distance=camera->GetDistance(); double x=0,y=0,z=0;
    if(orientation=="front") z=1; else if(orientation=="back") z=-1; else if(orientation=="right") x=1; else if(orientation=="left") x=-1; else if(orientation=="top") y=1; else if(orientation=="bottom") y=-1; else return;
    camera->SetPosition(focal[0]+x*distance,focal[1]+y*distance,focal[2]+z*distance);
    camera->SetViewUp(0, y==0?1:0, y==0?0:(y>0?-1:1)); impl_->renderer->ResetCameraClippingRange(); impl_->render();
}
void VtkSceneViewer::bindWorkbench(PostProcessWorkbenchWindow *workbench) {
    workbench->setDataServiceAvailable(true);
    connect(workbench,&PostProcessWorkbenchWindow::postProcessCommandRequested,this,[this](const QString &command) {
        if(command.startsWith(QStringLiteral("interactionMode:"))) { impl_->style->mode=command.mid(16); return; }
        if(!command.startsWith(QStringLiteral("orientationDrag:"))) return;
        const QStringList delta=command.mid(16).split(','); if(delta.size()!=2) return;
        bool okX=false,okY=false; const int x=delta[0].toInt(&okX),y=delta[1].toInt(&okY); if(!okX || !okY) return;
        auto *camera=impl_->renderer->GetActiveCamera(); camera->Azimuth(-x*.7); camera->Elevation(y*.7); camera->OrthogonalizeViewUp(); impl_->renderer->ResetCameraClippingRange(); impl_->render();
    });
    connect(workbench,&PostProcessWorkbenchWindow::backgroundChanged,this,[this](const QColor &bottom,const QColor &top,bool gradient) {
        impl_->renderer->SetBackground(bottom.redF(),bottom.greenF(),bottom.blueF()); impl_->renderer->SetBackground2(top.redF(),top.greenF(),top.blueF()); impl_->renderer->SetGradientBackground(gradient); impl_->render();
    });
    connect(workbench,&PostProcessWorkbenchWindow::datasetVisibilityChanged,this,[this](const QString &dataset,bool visible) {
        impl_->visibility[dataset]=visible;
        for(auto &display:impl_->displays) if(display.dataset==dataset) display.actor->SetVisibility(visible && display.active);
        if (impl_->legendDataset == dataset) {
            bool show = false;
            for (const auto &display : impl_->displays)
                if (display.dataset == dataset && display.region == impl_->legendRegion) show = display.actor->GetVisibility() != 0 && display.mapper->GetScalarVisibility() != 0;
            impl_->legend->setVisible(show);
        }
        impl_->render();
    });
    connect(workbench,&PostProcessWorkbenchWindow::regionVisibilityChanged,this,[this](const QString &dataset,const QString &region,bool visible) {
        for(auto &display:impl_->displays) if(display.dataset==dataset && display.region==region) { display.active=visible; display.actor->SetVisibility(visible && impl_->visibility.value(dataset,true)); }
        if (impl_->legendDataset == dataset && impl_->legendRegion == region)
            {
                bool show = false;
                for (const auto &display : impl_->displays) if (display.dataset == dataset && display.region == region)
                    show = display.actor->GetVisibility() && display.mapper->GetScalarVisibility();
                impl_->legend->setVisible(show);
            }
        impl_->render();
    });
    connect(workbench, &PostProcessWorkbenchWindow::viewRequested, this, [this](const QString &view) {
        if (view != QStringLiteral("focusSelected")) return;
        for (const auto &display : impl_->displays) {
            if (display.dataset == impl_->selectedDataset && display.region == impl_->selectedRegion && display.actor->GetVisibility()) {
                for (const auto &dataset : impl_->datasets) if (dataset.id == display.dataset)
                    for (const auto &region : dataset.data.regions) if (region.id == display.region && region.dimension == 2) {
                        if (impl_->orientEdgeOnRegion(region.data) && !display.customRepresentation) {
                            display.actor->GetProperty()->SetEdgeVisibility(true);
                            display.actor->GetProperty()->SetEdgeColor(.1, .12, .16);
                            display.actor->GetProperty()->SetLineWidth(3);
                        }
                    }
                impl_->renderer->ResetCamera(display.actor->GetBounds());
                impl_->renderer->ResetCameraClippingRange(); impl_->render(); return;
            }
        }
    });
    connect(workbench, &PostProcessWorkbenchWindow::exportImageRequested, this,
            [this, workbench](const QString &path, int width, int height) {
        impl_->render();
        const QPixmap capture = grab();
        const bool saved = !capture.isNull() && capture.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).save(path);
        workbench->presentImageExportResult(path, saved);
    });
    connect(workbench, &PostProcessWorkbenchWindow::fieldRangeChanged, this,
            [this](const QString &dataset, const QString &region, const QString &field, const QString &association, bool automatic, double minimum, double maximum) {
        for (auto &display : impl_->displays) if (display.dataset == dataset && display.region == region) {
            const QString key = association + QStringLiteral(":") + field;
            if (automatic) display.ranges.remove(key); else display.ranges.insert(key, qMakePair(minimum, maximum));
            break;
        }
        impl_->select(dataset, region, field, association);
    });
    connect(workbench, &PostProcessWorkbenchWindow::regionStyleChanged, this,
            [this](const QString &dataset, const QString &region, const QVariantMap &style) {
        for (auto &display : impl_->displays) if (display.dataset == dataset && display.region == region) {
            vtkProperty *property = display.actor->GetProperty();
            const int representation = style.value("representation").toInt();
            display.customRepresentation = true;
            property->SetPointSize(3.0);
            property->SetRepresentation(representation == 1 ? VTK_WIREFRAME : representation == 3 ? VTK_POINTS : VTK_SURFACE);
            property->SetEdgeVisibility(representation == 2);
            property->SetOpacity(qBound(0, style.value("opacity", 100).toInt(), 100) / 100.0);
            const int color = style.value("color").toInt();
            const double colors[][3] = {{.72,.78,.84},{.16,.38,.78},{.0,.7,.75},{1.,.55,.1},{.55,.55,.55}};
            const int index = qBound(0, color, 4);
            property->SetColor(colors[index][0], colors[index][1], colors[index][2]);
            if (index != 0) {
                display.mapper->ScalarVisibilityOff();
                if (impl_->legendDataset == dataset && impl_->legendRegion == region) impl_->legend->hide();
            } else if (!display.field.isEmpty()) {
                impl_->select(dataset, region, display.field, display.association);
            }
        }
        impl_->render();
    });
    connect(workbench,&PostProcessWorkbenchWindow::datasetRemovalRequested,this,[this](const QString &dataset) {
        for(int i=impl_->displays.size()-1;i>=0;--i) if(impl_->displays[i].dataset==dataset) { impl_->renderer->RemoveActor(impl_->displays[i].actor); impl_->displays.removeAt(i); }
        for(int i=impl_->datasets.size()-1;i>=0;--i) if(impl_->datasets[i].id==dataset) impl_->datasets.removeAt(i);
        impl_->visibility.remove(dataset);
        impl_->legend->hide();
        impl_->render();
    });
    connect(workbench,&PostProcessWorkbenchWindow::dataSelectionChanged,this,[this](const QString &dataset,const QString &region,const QString &field,const QString &association) { impl_->select(dataset,region,field,association); });
    connect(workbench,&PostProcessWorkbenchWindow::importDataRequested,this,[this,workbench](const QStringList &paths,const QString &mode) {
        if(impl_->busy) { workbench->presentImportError(QStringLiteral("A data import is already in progress.")); return; }
        impl_->busy=true;
        auto *watcher=new QFutureWatcher<BatchResult>(this);
        connect(watcher,&QFutureWatcher<BatchResult>::finished,this,[this,watcher,workbench,mode] {
            const auto result=watcher->result(); watcher->deleteLater(); impl_->busy=false;
            if(!result.error.isEmpty()) { workbench->presentImportError(result.error); return; }
            const bool first = impl_->datasets.isEmpty() || mode=="replace";
            if(mode=="replace") { impl_->renderer->RemoveAllViewProps(); impl_->legend->hide(); impl_->displays.clear(); impl_->datasets.clear(); impl_->visibility.clear(); }
            QVariantList description;
            for(const auto &data:result.datasets) {
                LoadedDataset loaded; loaded.id=QUuid::createUuid().toString(); loaded.data=data; impl_->datasets.append(loaded); impl_->addDataset(loaded);
                QVariantMap dataset; dataset["id"]=loaded.id; dataset["path"]=data.path; dataset["name"]=QFileInfo(data.path).fileName(); QVariantList regions;
                for(const auto &region:data.regions) {
                    QVariantMap rm; rm["id"]=region.id; rm["name"]=region.name; rm["dimension"]=region.dimension; rm["kind"]=region.kind; QVariantList fields;
                    for(const auto &field:region.fields) { QVariantMap f; f["name"]=field.name; f["association"]=field.association; f["components"]=field.components; f["hasRange"]=field.hasRange; f["minimum"]=field.minimum; f["maximum"]=field.maximum; fields.append(f); }
                    rm["fields"]=fields; regions.append(rm);
                }
                dataset["regions"]=regions; description.append(dataset);
            }
            if(first) impl_->renderer->ResetCamera(); impl_->render();
            workbench->presentImportedData(description,mode=="replace");
        });
        watcher->setFuture(QtConcurrent::run([paths] {
            BatchResult batch; auto adapter=createCgnsAdapter();
            for(const auto &path:paths) {
                if(!adapter->accepts(path)) { batch.error=QStringLiteral("Unsupported data format: ")+path; break; }
                const auto result=adapter->read(path); if(!result.ok()) { batch.error=result.error; break; }
                batch.datasets.append(result.dataset);
            }
            return batch;
        }));
    });
}
} }
