#include <QApplication>
#include <QSurfaceFormat>
#include <QtGlobal>

#include <QVTKOpenGLNativeWidget.h>
#include <SARibbonMainWindow.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>
#include <vtkVersion.h>

#ifdef AEROSIM_VTK_DEBUG_ONLY
#error "The selected VTK package exports Debug only; use Debug or install a matching Release VTK 8.2.0 build"
#endif

static_assert(QT_VERSION == QT_VERSION_CHECK(5, 12, 12), "Qt 5.12.12 is required");
static_assert(VTK_MAJOR_VERSION == 8 && VTK_MINOR_VERSION == 2 && VTK_BUILD_VERSION == 0,
              "VTK 8.2.0 is required");
static_assert(sizeof(void *) == 8, "64-bit toolchain is required");

int main(int argc, char **argv)
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);
    SARibbonMainWindow window;
    auto *viewport = new QVTKOpenGLNativeWidget(&window);
    vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
    viewport->SetRenderWindow(renderWindow.GetPointer());
    window.setCentralWidget(viewport);
    return viewport->GetRenderWindow() == renderWindow.GetPointer() ? 0 : 1;
}
