#include "MeshViewerBinding.h"

#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/MeshManagementPage.h"
#include "aerosim/visualization/data/BoundaryLoadService.h"
#include "aerosim/visualization/scene/MeshSceneViewer.h"

namespace aerosim {
using visualization::BoundaryLoadService;
using visualization::MeshSceneViewer;

void configureMeshViewerSurface()
{
    MeshSceneViewer::configureSurface();
}

bool installMeshViewer(MainWindow &window)
{
    auto *page = window.findChild<MeshManagementPage *>();
    if (!page) return false;

    auto *viewer = new MeshSceneViewer;
    if (!window.insertWidget(WidgetSlot::Viewport, viewer)) {
        delete viewer;
        return false;
    }

    auto *service = new BoundaryLoadService(viewer);
    QObject::connect(page, &MeshManagementPage::boundaryImportRequested,
                     service, &BoundaryLoadService::loadFiles);
    QObject::connect(page, &MeshManagementPage::boundaryRemoveRequested,
                     service, &BoundaryLoadService::remove);
    QObject::connect(page, &MeshManagementPage::boundaryRemoveRequested,
                     viewer, &MeshSceneViewer::removeObject);
    QObject::connect(page, &MeshManagementPage::boundaryStyleChanged,
                     viewer, &MeshSceneViewer::setObjectStyle);
    QObject::connect(page, &MeshManagementPage::boundaryViewRequested,
                     viewer, &MeshSceneViewer::setCameraOrientation);
    QObject::connect(page, &MeshManagementPage::boundaryOrbitRequested,
                     viewer, &MeshSceneViewer::orbitCamera);
    QObject::connect(page, &MeshManagementPage::boundaryAxisRotationRequested,
                     viewer, &MeshSceneViewer::rotateObjectAroundAxis);
    QObject::connect(page, &MeshManagementPage::boundaryDragRotationRequested,
                     viewer, &MeshSceneViewer::rotateObjectByDrag);
    QObject::connect(viewer, &MeshSceneViewer::boundaryPicked,
                     page, &MeshManagementPage::selectBoundary);
    QObject::connect(viewer, &MeshSceneViewer::cameraOrientationChanged,
                     page, &MeshManagementPage::setBoundaryCameraOrientation);
    QObject::connect(page, &MeshManagementPage::boundaryBackgroundRequested, viewer, [viewer](int mode) {
        const QColor top = mode == 1 ? QColor("#2f3b49")
                           : mode == 2 ? QColor("#565c77") : QColor("#b8a9e3");
        const QColor bottom = mode == 1 ? QColor("#4a5a6d")
                              : mode == 2 ? QColor("#ffffff") : QColor("#f8f5fc");
        viewer->setBackground(bottom, top, true);
    });
    QObject::connect(service, &BoundaryLoadService::started,
                     page, &MeshManagementPage::beginBoundary);
    QObject::connect(service, &BoundaryLoadService::loaded, page,
                     [page, viewer, service](const QString &id, const QString &, const QString &summary) {
        viewer->addBoundary(id, service->mesh(id));
        page->finishBoundary(id, summary);
    });
    QObject::connect(service, &BoundaryLoadService::failed,
                     page, &MeshManagementPage::failBoundary);
    QObject::connect(service, &BoundaryLoadService::busyChanged,
                     page, &MeshManagementPage::setBoundaryBusy);

#ifdef AEROSIM_MESH_HAS_CGNS
    page->enableBoundaryMode(true);
#else
    page->enableBoundaryMode();
#endif
    viewer->setCameraOrientation(QStringLiteral("top"));
    return true;
}
} // namespace aerosim
