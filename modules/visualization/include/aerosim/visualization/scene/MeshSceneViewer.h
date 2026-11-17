#pragma once
#include <QWidget>
#include <QColor>
#include <QVariantMap>
#include <QVector3D>
#include <memory>
#include "aerosim/visualization/features/BoundarySurfaceExtractor.h"
namespace aerosim { namespace visualization {
class MeshSceneViewer final : public QWidget {
    Q_OBJECT
public:
    explicit MeshSceneViewer(QWidget* parent=nullptr);
    ~MeshSceneViewer() override;
    void addBoundary(const QString& id,std::shared_ptr<const BoundaryMesh> mesh);
    static void configureSurface();
public slots:
    void removeObject(const QString& id);
    void setObjectStyle(const QString& id,const QVariantMap& style);
    void setBackground(const QColor& bottom,const QColor& top,bool gradient);
    void setCameraOrientation(const QString& command);
    void orbitCamera(int dx, int dy);
    void rotateObjectAroundAxis(const QString &id, const QVector3D &axis);
    void rotateObjectByDrag(const QString &id, int dx, int dy);
signals:
    void boundaryPicked(const QString &id);
    void cameraOrientationChanged(const QVector3D &from, const QVector3D &up);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    void render();
    void publishCameraOrientation();
    void pickBoundary(const QPoint &position);
};
} }
