#pragma once
#include <QWidget>
#include <memory>
class QColor;
class QResizeEvent;
class QString;
namespace aerosim { class PostProcessWorkbenchWindow; namespace visualization {
class VtkSceneViewer : public QWidget {
public:
    explicit VtkSceneViewer(QWidget *parent = nullptr);
    ~VtkSceneViewer() override;
    void bindWorkbench(PostProcessWorkbenchWindow *window);
    void resetCamera();
    void setCameraOrientation(const QString &orientation);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} }
