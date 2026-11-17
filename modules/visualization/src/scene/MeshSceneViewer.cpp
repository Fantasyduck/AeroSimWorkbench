#include "aerosim/visualization/scene/MeshSceneViewer.h"
#include <QVTKOpenGLNativeWidget.h>
#include <QApplication>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QThread>
#include <QSurfaceFormat>
#include <QHash>
#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCallbackCommand.h>
#include <vtkCellPicker.h>
#include <vtkCommand.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <array>
#include <cmath>
namespace aerosim { namespace visualization {
struct MeshSceneViewer::Impl {
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> window=vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    vtkSmartPointer<vtkRenderer> renderer=vtkSmartPointer<vtkRenderer>::New();
    vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
        vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    unsigned long interactionObserver = 0;
    QHash<QString, vtkSmartPointer<vtkActor>> objects;
    QVTKOpenGLNativeWidget *viewport = nullptr;
    QPoint pickPressPosition;
    bool pickPending = false;
};
MeshSceneViewer::MeshSceneViewer(QWidget* parent):QWidget(parent),impl_(new Impl) {
    setObjectName(QStringLiteral("boundaryVtkViewer"));
    auto* layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0);
    auto* widget=new QVTKOpenGLNativeWidget(this); layout->addWidget(widget);
    impl_->window->AddRenderer(impl_->renderer); widget->SetRenderWindow(impl_->window);
    impl_->viewport = widget;
    widget->installEventFilter(this);
    impl_->window->GetInteractor()->SetInteractorStyle(impl_->style);
    vtkNew<vtkCallbackCommand> interaction;
    interaction->SetClientData(this);
    interaction->SetCallback([](vtkObject *, unsigned long, void *client, void *) {
        static_cast<MeshSceneViewer *>(client)->publishCameraOrientation();
    });
    impl_->interactionObserver = impl_->style->AddObserver(vtkCommand::InteractionEvent, interaction);
    impl_->renderer->SetBackground(0.15,0.19,0.25);
    impl_->renderer->SetBackground2(0.85,0.9,0.96); impl_->renderer->GradientBackgroundOn();
}
MeshSceneViewer::~MeshSceneViewer()
{
    impl_->viewport->removeEventFilter(this);
    impl_->style->RemoveObserver(impl_->interactionObserver);
}

void MeshSceneViewer::configureSurface()
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
}
void MeshSceneViewer::render() {
    Q_ASSERT(QThread::currentThread()==thread());
    if(isVisible()) impl_->window->Render();
}
void MeshSceneViewer::addBoundary(const QString& id,std::shared_ptr<const BoundaryMesh> mesh) {
    if(!mesh || impl_->objects.contains(id)) return;
    vtkNew<vtkPolyDataMapper> mapper; mapper->SetInputData(mesh->surface); mapper->ScalarVisibilityOff();
    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    double center[3];
    mesh->surface->GetCenter(center);
    actor->SetOrigin(center);
    actor->GetProperty()->SetEdgeColor(0.1, 0.15, 0.2);
    impl_->renderer->AddActor(actor);
    impl_->objects.insert(id, actor);
    if (impl_->objects.size() == 1) setCameraOrientation(QStringLiteral("fit"));
    else {
        impl_->renderer->ResetCameraClippingRange();
        render();
    }
}
void MeshSceneViewer::removeObject(const QString& id) {
    if(!impl_->objects.contains(id)) return;
    impl_->renderer->RemoveActor(impl_->objects.take(id)); render();
}
void MeshSceneViewer::setObjectStyle(const QString& id,const QVariantMap& s) {
    if(!impl_->objects.contains(id)) return;
    auto* a=impl_->objects.value(id).GetPointer(); auto* p=a->GetProperty();
    if(s.contains("visible")) a->SetVisibility(s.value("visible").toBool());
    if(s.contains("color")) { QColor c=s.value("color").value<QColor>(); if(c.isValid()) p->SetColor(c.redF(),c.greenF(),c.blueF()); }
    if(s.contains("opacity")) p->SetOpacity(qBound(0.0,s.value("opacity").toDouble()/100.0,1.0));
    if (s.contains("scale")) {
        const double scale = s.value("scale").toDouble();
        if (std::isfinite(scale) && scale > 0) {
            const auto *origin = a->GetOrigin();
            // Retain world-origin scaling while using the mesh center as the rotation pivot.
            a->SetPosition((scale - 1) * origin[0], (scale - 1) * origin[1], (scale - 1) * origin[2]);
            a->SetScale(scale);
        }
    }
    if(s.contains("mode")) {
        const int m=s.value("mode").toInt();
        p->SetRepresentation(m==1?VTK_WIREFRAME:VTK_SURFACE); p->SetEdgeVisibility(m==2);
    }
    impl_->renderer->ResetCameraClippingRange(); render();
}
void MeshSceneViewer::setBackground(const QColor& b,const QColor& t,bool g) {
    impl_->renderer->SetBackground(b.redF(),b.greenF(),b.blueF());
    impl_->renderer->SetBackground2(t.redF(),t.greenF(),t.blueF()); impl_->renderer->SetGradientBackground(g); render();
}
void MeshSceneViewer::setCameraOrientation(const QString &command)
{
    auto *camera = impl_->renderer->GetActiveCamera();
    if (command == QStringLiteral("fit")) {
        impl_->renderer->ResetCamera();
    } else if (command == QStringLiteral("in") || command == QStringLiteral("out")) {
        camera->Dolly(command == QStringLiteral("in") ? 1.2 : 1.0 / 1.2);
    } else {
        struct View { const char *name; QVector3D from; QVector3D up; };
        const std::array<View, 7> views = {{
            {"front", {0, -1, 0}, {0, 0, 1}},
            {"back", {0, 1, 0}, {0, 0, 1}},
            {"right", {1, 0, 0}, {0, 0, 1}},
            {"left", {-1, 0, 0}, {0, 0, 1}},
            {"top", {0, 0, 1}, {0, 1, 0}},
            {"bottom", {0, 0, -1}, {0, 1, 0}},
            {"iso", {1, -1, 1}, {0, 0, 1}},
        }};
        for (const auto &view : views) {
            if (command != QLatin1String(view.name)) continue;
            const auto *center = camera->GetFocalPoint();
            const auto offset = view.from.normalized();
            const double distance = camera->GetDistance();
            camera->SetPosition(center[0] + distance * offset.x(),
                                center[1] + distance * offset.y(),
                                center[2] + distance * offset.z());
            camera->SetViewUp(view.up.x(), view.up.y(), view.up.z());
            camera->OrthogonalizeViewUp();
            break;
        }
    }
    impl_->renderer->ResetCameraClippingRange();
    publishCameraOrientation();
    render();
}

void MeshSceneViewer::orbitCamera(int dx, int dy)
{
    auto *camera = impl_->renderer->GetActiveCamera();
    constexpr double degreesPerPixel = 0.65;
    camera->Azimuth(-dx * degreesPerPixel);
    camera->Elevation(-dy * degreesPerPixel);
    camera->OrthogonalizeViewUp();
    impl_->renderer->ResetCameraClippingRange();
    publishCameraOrientation();
    render();
}

void MeshSceneViewer::rotateObjectAroundAxis(const QString &id, const QVector3D &axis)
{
    auto actor = impl_->objects.value(id);
    if (!actor || !actor->GetVisibility()) return;
    actor->RotateWXYZ(90.0, axis.x(), axis.y(), axis.z());
    impl_->renderer->ResetCameraClippingRange();
    render();
}

void MeshSceneViewer::rotateObjectByDrag(const QString &id, int dx, int dy)
{
    auto actor = impl_->objects.value(id);
    if (!actor || !actor->GetVisibility()) return;
    auto *camera = impl_->renderer->GetActiveCamera();
    const auto *direction = camera->GetDirectionOfProjection();
    const auto *viewUp = camera->GetViewUp();
    const QVector3D forward(direction[0], direction[1], direction[2]);
    const QVector3D up(viewUp[0], viewUp[1], viewUp[2]);
    const auto right = QVector3D::crossProduct(forward, up).normalized();
    const auto screenUp = QVector3D::crossProduct(right, forward).normalized();
    constexpr double degreesPerPixel = 0.65;
    if (dx) actor->RotateWXYZ(dx * degreesPerPixel, screenUp.x(), screenUp.y(), screenUp.z());
    if (dy) actor->RotateWXYZ(dy * degreesPerPixel, right.x(), right.y(), right.z());
    impl_->renderer->ResetCameraClippingRange();
    render();
}

bool MeshSceneViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != impl_->viewport) return QWidget::eventFilter(watched, event);
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        impl_->pickPending = mouse->button() == Qt::LeftButton && mouse->modifiers() == Qt::NoModifier;
        impl_->pickPressPosition = mouse->pos();
        break;
    }
    case QEvent::MouseMove:
        if ((static_cast<QMouseEvent *>(event)->pos() - impl_->pickPressPosition).manhattanLength()
            >= QApplication::startDragDistance()) impl_->pickPending = false;
        break;
    case QEvent::MouseButtonRelease: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const bool isClick = impl_->pickPending && mouse->button() == Qt::LeftButton
            && mouse->modifiers() == Qt::NoModifier && impl_->viewport->rect().contains(mouse->pos())
            && (mouse->pos() - impl_->pickPressPosition).manhattanLength() < QApplication::startDragDistance();
        impl_->pickPending = false;
        if (isClick) pickBoundary(mouse->pos());
        break;
    }
    case QEvent::UngrabMouse:
        impl_->pickPending = false;
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

void MeshSceneViewer::pickBoundary(const QPoint &position)
{
    const auto *size = impl_->window->GetSize();
    if (size[0] <= 0 || size[1] <= 0 || impl_->viewport->width() <= 0 || impl_->viewport->height() <= 0) return;
    // Qt uses logical pixels from the top-left. VTK uses framebuffer pixels from the bottom-left.
    const double x = position.x() * double(size[0]) / impl_->viewport->width();
    const double y = size[1] - 1 - position.y() * double(size[1]) / impl_->viewport->height();
    vtkNew<vtkCellPicker> picker;
    picker->Pick(x, y, 0, impl_->renderer);
    auto *pickedActor = picker->GetActor();
    for (auto item = impl_->objects.cbegin(); item != impl_->objects.cend(); ++item) {
        if (item.value().GetPointer() != pickedActor) continue;
        emit boundaryPicked(item.key());
        return;
    }
    emit boundaryPicked(QString());
}

void MeshSceneViewer::publishCameraOrientation()
{
    auto *camera = impl_->renderer->GetActiveCamera();
    const auto *direction = camera->GetDirectionOfProjection();
    const auto *up = camera->GetViewUp();
    emit cameraOrientationChanged(QVector3D(-direction[0], -direction[1], -direction[2]),
                                  QVector3D(up[0], up[1], up[2]));
}
} }
