"""Standalone Qt/VTK post-processing UI prototype with synthetic flow data."""

import sys

import numpy as np
from PySide2.QtCore import Qt, QTimer
from PySide2.QtGui import QKeySequence
from PySide2.QtWidgets import (
    QAction,
    QApplication,
    QButtonGroup,
    QCheckBox,
    QComboBox,
    QFileDialog,
    QFormLayout,
    QFrame,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSlider,
    QSplitter,
    QTabWidget,
    QTextEdit,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)
from vtkmodules.qt.QVTKRenderWindowInteractor import QVTKRenderWindowInteractor
from vtkmodules.vtkCommonCore import vtkLookupTable
from vtkmodules.vtkCommonDataModel import vtkImageData, vtkPlane
from vtkmodules.vtkCommonMath import vtkRungeKutta4
from vtkmodules.vtkCommonTransforms import vtkTransform
from vtkmodules.vtkFiltersCore import vtkContourFilter, vtkCutter, vtkExtractEdges, vtkProbeFilter, vtkTubeFilter
from vtkmodules.vtkFiltersFlowPaths import vtkStreamTracer
from vtkmodules.vtkFiltersGeneral import vtkTransformPolyDataFilter
from vtkmodules.vtkFiltersModeling import vtkOutlineFilter
from vtkmodules.vtkFiltersSources import vtkLineSource, vtkSphereSource
from vtkmodules.vtkIOImage import vtkPNGWriter
from vtkmodules.vtkIOXML import vtkXMLPolyDataWriter
from vtkmodules.vtkInteractionStyle import vtkInteractorStyleTrackballCamera
from vtkmodules.vtkRenderingAnnotation import vtkScalarBarActor
from vtkmodules.vtkRenderingCore import vtkActor, vtkCellPicker, vtkPolyDataMapper, vtkRenderer, vtkWindowToImageFilter
import vtkmodules.vtkRenderingOpenGL2  # noqa: F401 - register OpenGL rendering backend
import vtkmodules.vtkRenderingFreeType  # noqa: F401 - register text rendering backend
from vtkmodules.util.numpy_support import numpy_to_vtk


VARIABLES = {
    "Pressure": ("压力", "kPa"),
    "Temperature": ("温度", "K"),
    "Velocity": ("速度", "m/s"),
}


def synthetic_flow() -> vtkImageData:
    """Create a small 3D vector field and three point scalar arrays."""
    dims = (61, 49, 41)
    origin = (-2.4, -1.7, -1.35)
    spacing = (4.8 / (dims[0] - 1), 3.4 / (dims[1] - 1), 2.7 / (dims[2] - 1))
    x, y, z = np.meshgrid(
        origin[0] + np.arange(dims[0]) * spacing[0],
        origin[1] + np.arange(dims[1]) * spacing[1],
        origin[2] + np.arange(dims[2]) * spacing[2],
        indexing="ij",
    )
    r2 = (y / 0.58) ** 2 + (z / 0.48) ** 2
    wake = np.exp(-((x - 0.9) / 1.15) ** 2 - r2)
    bow = np.exp(-((x + 1.15) / 0.55) ** 2 - r2 * 0.7)
    pressure = 101.3 + 28 * bow - 20 * wake + 3.5 * np.sin(1.5 * y + 0.8 * z)
    temperature = 294 + 31 * bow + 14 * wake + 5 * np.cos(1.5 * z)
    u = 145 - 42 * bow - 28 * wake
    v = 18 * y * wake - 7 * z * bow
    w = 15 * z * wake + 5 * y * bow
    velocity = np.sqrt(u * u + v * v + w * w)

    image = vtkImageData()
    image.SetDimensions(*dims)
    image.SetOrigin(*origin)
    image.SetSpacing(*spacing)
    for name, values in (
        ("Pressure", pressure),
        ("Temperature", temperature),
        ("Velocity", velocity),
    ):
        array = numpy_to_vtk(np.ascontiguousarray(values.ravel(order="F")), deep=True)
        array.SetName(name)
        image.GetPointData().AddArray(array)
    vectors = np.column_stack((u.ravel(order="F"), v.ravel(order="F"), w.ravel(order="F")))
    vector_array = numpy_to_vtk(np.ascontiguousarray(vectors), deep=True)
    vector_array.SetName("FlowVector")
    image.GetPointData().SetVectors(vector_array)
    image.GetPointData().SetActiveScalars("Pressure")
    return image


def color_table(style: str) -> vtkLookupTable:
    stops = {
        "经典彩虹": [(0.10, 0.20, 0.72), (0.10, 0.70, 0.95), (0.24, 0.79, 0.46), (0.98, 0.84, 0.20), (0.82, 0.13, 0.13)],
        "冷暖色": [(0.20, 0.32, 0.72), (0.62, 0.77, 0.94), (0.97, 0.97, 0.95), (0.96, 0.64, 0.48), (0.68, 0.08, 0.15)],
        "灰度": [(0.13, 0.16, 0.21), (0.35, 0.38, 0.42), (0.57, 0.60, 0.64), (0.79, 0.81, 0.84), (0.98, 0.98, 0.98)],
    }[style]
    lut = vtkLookupTable()
    lut.SetNumberOfTableValues(256)
    lut.Build()
    for i in range(256):
        t = i / 255 * (len(stops) - 1)
        j = min(int(t), len(stops) - 2)
        a = t - j
        rgb = [(1 - a) * stops[j][c] + a * stops[j + 1][c] for c in range(3)]
        lut.SetTableValue(i, *rgb, 1.0)
    return lut


class VTKViewer(QVTKRenderWindowInteractor):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.data = synthetic_flow()
        self.renderer = vtkRenderer()
        self.renderer.SetBackground(0.965, 0.975, 0.99)
        self.GetRenderWindow().AddRenderer(self.renderer)
        self.interactor = self.GetRenderWindow().GetInteractor()
        self.interactor.SetInteractorStyle(vtkInteractorStyleTrackballCamera())
        self.interactor.AddObserver("LeftButtonPressEvent", self._on_click, 1.0)
        self.pick_enabled = False
        self.pick_callback = None
        self.mode = "切面"
        self.variable = "Pressure"
        self.axis = "Z"
        self.position = 0.5
        self.iso_fraction = 0.55
        self.show_mesh = False
        self.palette = "经典彩虹"
        self.active_polydata = None
        self.actors = []

        self.update_scene(reset_camera=True, render=False)

    def _surface(self):
        sphere = vtkSphereSource()
        sphere.SetThetaResolution(72)
        sphere.SetPhiResolution(44)
        transform = vtkTransform()
        transform.Scale(1.42, 0.34, 0.31)
        geometry = vtkTransformPolyDataFilter()
        geometry.SetTransform(transform)
        geometry.SetInputConnection(sphere.GetOutputPort())
        probe = vtkProbeFilter()
        probe.SetInputConnection(geometry.GetOutputPort())
        probe.SetSourceData(self.data)
        probe.Update()
        return probe.GetOutput()

    def _mapper(self, polydata, lut, value_range):
        mapper = vtkPolyDataMapper()
        mapper.SetInputData(polydata)
        mapper.SetScalarModeToUsePointFieldData()
        mapper.SelectColorArray(self.variable)
        mapper.SetLookupTable(lut)
        mapper.SetScalarRange(*value_range)
        mapper.ScalarVisibilityOn()
        return mapper

    def _actor(self, polydata, lut, value_range, opacity=1.0, width=1.0):
        actor = vtkActor()
        actor.SetMapper(self._mapper(polydata, lut, value_range))
        actor.GetProperty().SetOpacity(opacity)
        actor.GetProperty().SetLineWidth(width)
        self.renderer.AddActor(actor)
        self.actors.append(actor)
        return actor

    def update_scene(self, reset_camera=False, render=True):
        for actor in self.actors:
            self.renderer.RemoveActor(actor)
        self.actors.clear()
        self.renderer.RemoveAllViewProps()
        values = self.data.GetPointData().GetArray(self.variable)
        value_range = values.GetRange()
        lut = color_table(self.palette)
        lut.SetRange(*value_range)
        surface = self._surface()
        self.active_polydata = surface

        if self.mode == "云图":
            self._actor(surface, lut, value_range)
        else:
            body = self._actor(surface, lut, value_range, opacity=0.16)
            body.GetProperty().SetEdgeVisibility(False)
            if self.mode == "切面":
                plane = vtkPlane()
                axis_index = {"X": 0, "Y": 1, "Z": 2}[self.axis]
                bounds = self.data.GetBounds()
                low, high = bounds[axis_index * 2:axis_index * 2 + 2]
                point = [0.0, 0.0, 0.0]
                point[axis_index] = low + self.position * (high - low)
                normal = [0.0, 0.0, 0.0]
                normal[axis_index] = 1.0
                plane.SetOrigin(*point)
                plane.SetNormal(*normal)
                cutter = vtkCutter()
                cutter.SetCutFunction(plane)
                cutter.SetInputData(self.data)
                cutter.Update()
                self.active_polydata = cutter.GetOutput()
                self._actor(self.active_polydata, lut, value_range)
            elif self.mode == "等值面":
                contour = vtkContourFilter()
                contour.SetInputData(self.data)
                contour.SetInputArrayToProcess(0, 0, 0, 0, self.variable)
                contour.SetValue(0, value_range[0] + self.iso_fraction * (value_range[1] - value_range[0]))
                contour.Update()
                self.active_polydata = contour.GetOutput()
                self._actor(self.active_polydata, lut, value_range, opacity=0.82)
            elif self.mode == "流线":
                seeds = vtkLineSource()
                seeds.SetPoint1(-1.9, -1.2, 0.0)
                seeds.SetPoint2(-1.9, 1.2, 0.0)
                seeds.SetResolution(22)
                trace = vtkStreamTracer()
                trace.SetInputData(self.data)
                trace.SetSourceConnection(seeds.GetOutputPort())
                trace.SetInputArrayToProcess(0, 0, 0, 0, "FlowVector")
                trace.SetMaximumPropagation(6.0)
                trace.SetInitialIntegrationStep(0.03)
                trace.SetIntegratorTypeToRungeKutta4()
                trace.SetIntegrationDirectionToForward()
                trace.Update()
                tube = vtkTubeFilter()
                tube.SetInputConnection(trace.GetOutputPort())
                tube.SetRadius(0.013)
                tube.SetNumberOfSides(7)
                tube.Update()
                self.active_polydata = tube.GetOutput()
                self._actor(self.active_polydata, lut, value_range)

        if self.show_mesh:
            edge = vtkExtractEdges()
            edge.SetInputData(surface)
            edge.Update()
            mesh_actor = vtkActor()
            mesh_mapper = vtkPolyDataMapper()
            mesh_mapper.SetInputConnection(edge.GetOutputPort())
            mesh_actor.SetMapper(mesh_mapper)
            mesh_actor.GetProperty().SetColor(0.20, 0.25, 0.33)
            mesh_actor.GetProperty().SetOpacity(0.55)
            mesh_actor.GetProperty().SetLineWidth(0.7)
            self.renderer.AddActor(mesh_actor)
            self.actors.append(mesh_actor)

        outline = vtkOutlineFilter()
        outline.SetInputData(self.data)
        outline_actor = vtkActor()
        outline_mapper = vtkPolyDataMapper()
        outline_mapper.SetInputConnection(outline.GetOutputPort())
        outline_actor.SetMapper(outline_mapper)
        outline_actor.GetProperty().SetColor(0.68, 0.73, 0.80)
        outline_actor.GetProperty().SetLineWidth(1.0)
        self.renderer.AddActor(outline_actor)
        self.actors.append(outline_actor)

        self.scalar_bar = vtkScalarBarActor()
        self.scalar_bar.SetLookupTable(lut)
        self.scalar_bar.SetTitle(self.variable)
        self.scalar_bar.SetLabelFormat("%.1f")
        self.scalar_bar.SetNumberOfLabels(5)
        self.scalar_bar.SetWidth(0.08)
        self.scalar_bar.SetHeight(0.48)
        self.scalar_bar.SetPosition(0.82, 0.25)
        self.scalar_bar.SetUnconstrainedFontSize(True)
        self.scalar_bar.GetTitleTextProperty().SetColor(0.18, 0.22, 0.29)
        self.scalar_bar.GetTitleTextProperty().SetFontSize(14)
        self.scalar_bar.GetTitleTextProperty().BoldOff()
        self.scalar_bar.GetLabelTextProperty().SetColor(0.18, 0.22, 0.29)
        self.scalar_bar.GetLabelTextProperty().SetFontSize(12)
        self.scalar_bar.GetLabelTextProperty().BoldOff()
        self.renderer.AddViewProp(self.scalar_bar)
        if reset_camera:
            self.renderer.ResetCamera()
            self.renderer.GetActiveCamera().Azimuth(35)
            self.renderer.GetActiveCamera().Elevation(20)
            self.renderer.ResetCameraClippingRange()
        if render:
            self.GetRenderWindow().Render()

    def set_view(self, direction):
        camera = self.renderer.GetActiveCamera()
        focal = (0.0, 0.0, 0.0)
        distance = 9.0
        positions = {
            "X": (distance, 0, 0),
            "Y": (0, distance, 0),
            "Z": (0, 0, distance),
            "ISO": (distance * 0.8, distance * 0.7, distance * 0.6),
        }
        camera.SetFocalPoint(*focal)
        camera.SetPosition(*positions[direction])
        camera.SetViewUp(0, 1, 0) if direction == "Z" else camera.SetViewUp(0, 0, 1)
        self.renderer.ResetCamera()
        self.renderer.ResetCameraClippingRange()
        self.GetRenderWindow().Render()

    def _on_click(self, interactor, event):
        if not self.pick_enabled:
            return
        x, y = self.interactor.GetEventPosition()
        picker = vtkCellPicker()
        picker.SetTolerance(0.005)
        if picker.Pick(x, y, 0, self.renderer):
            point = picker.GetPickPosition()
            pid = self.data.FindPoint(point)
            if pid >= 0 and self.pick_callback:
                value = self.data.GetPointData().GetArray(self.variable).GetTuple1(pid)
                self.pick_callback(point, value)

    def export_png(self, path: str):
        capture = vtkWindowToImageFilter()
        capture.SetInput(self.GetRenderWindow())
        capture.SetScale(2)
        capture.Update()
        writer = vtkPNGWriter()
        writer.SetFileName(path)
        writer.SetInputConnection(capture.GetOutputPort())
        writer.Write()

    def export_vtp(self, path: str):
        writer = vtkXMLPolyDataWriter()
        writer.SetFileName(path)
        writer.SetInputData(self.active_polydata)
        if not writer.Write():
            raise OSError("VTP 文件写入失败")


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("后处理工作台 · VTK Demo")
        self.resize(1510, 940)
        self.setMinimumSize(1140, 720)
        self._build_ui()
        self._connect()
        self._log("已加载示例任务 001：合成三维流场，61 × 49 × 41 点。")
        self._log("拖动旋转，滚轮缩放；开启拾取后单击模型查看局部数值。")

    def showEvent(self, event):
        super().showEvent(event)
        # Wait for Qt to finish creating the native viewport before enabling
        # interaction; enabling during the first paint loops on macOS.
        QTimer.singleShot(200, self.viewer.interactor.Enable)

    def _build_ui(self):
        root = QWidget()
        root_layout = QVBoxLayout(root)
        root_layout.setContentsMargins(0, 0, 0, 0)
        root_layout.setSpacing(0)

        header = QFrame()
        header.setObjectName("header")
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(18, 10, 18, 10)
        title = QLabel("后处理工作台")
        title.setObjectName("title")
        header_layout.addWidget(title)
        tag = QLabel("DEMO  /  SYNTHETIC DATA")
        tag.setObjectName("tag")
        header_layout.addWidget(tag)
        header_layout.addStretch()
        header_layout.addWidget(QLabel("当前任务"))
        self.task_combo = QComboBox()
        self.task_combo.addItem("示例任务 001 · 已加载")
        self.task_combo.setMinimumWidth(210)
        header_layout.addWidget(self.task_combo)
        root_layout.addWidget(header)

        self.ribbon = QTabWidget()
        self.ribbon.setObjectName("ribbon")
        self.ribbon.setFixedHeight(113)
        self.ribbon.addTab(self._ribbon_results(), "结果")
        self.ribbon.addTab(self._ribbon_analysis(), "分析")
        self.ribbon.addTab(self._ribbon_views(), "视图")
        root_layout.addWidget(self.ribbon)

        vertical = QSplitter(Qt.Vertical)
        horizontal = QSplitter(Qt.Horizontal)
        horizontal.addWidget(self._tree_panel())
        self.viewer = VTKViewer()
        horizontal.addWidget(self.viewer)
        horizontal.addWidget(self._properties_panel())
        horizontal.setSizes([238, 920, 300])
        horizontal.setStretchFactor(1, 1)
        vertical.addWidget(horizontal)
        vertical.addWidget(self._output_panel())
        vertical.setSizes([650, 160])
        root_layout.addWidget(vertical, 1)
        self.setCentralWidget(root)

        self.status = QLabel("就绪  |  示例任务 001  |  Pressure  |  切面")
        self.statusBar().addWidget(self.status, 1)
        self.pick_status = QLabel("拾取：关闭")
        self.statusBar().addPermanentWidget(self.pick_status)
        self._build_menu()

    def _ribbon_button(self, layout, text, callback=None, checkable=False):
        button = QPushButton(text)
        button.setProperty("ribbonButton", True)
        button.setCheckable(checkable)
        button.setMinimumWidth(78)
        button.setMinimumHeight(48)
        layout.addWidget(button)
        if callback:
            button.clicked.connect(callback)
        return button

    def _ribbon_page(self):
        page = QWidget()
        layout = QHBoxLayout(page)
        layout.setContentsMargins(14, 6, 14, 8)
        layout.setSpacing(8)
        return page, layout

    def _ribbon_results(self):
        page, row = self._ribbon_page()
        self._ribbon_button(row, "导出 PNG", self._export_png)
        self._ribbon_button(row, "导出 VTP", self._export_vtp)
        row.addWidget(self._separator())
        self._ribbon_button(row, "适合窗口", lambda: self.viewer.set_view("ISO"))
        self.pick_button = self._ribbon_button(row, "拾取数值", self._toggle_pick, True)
        row.addStretch()
        row.addWidget(QLabel("数据源：内置合成流场  ·  单帧"))
        return page

    def _ribbon_analysis(self):
        page, row = self._ribbon_page()
        self.mode_group = QButtonGroup(self)
        self.mode_group.setExclusive(True)
        self.mode_buttons = {}
        for mode in ("云图", "切面", "等值面", "流线"):
            button = self._ribbon_button(row, mode, checkable=True)
            self.mode_group.addButton(button)
            self.mode_buttons[mode] = button
            button.clicked.connect(lambda checked=False, m=mode: self._set_mode(m))
        self.mode_buttons["切面"].setChecked(True)
        row.addWidget(self._separator())
        self.mesh_button = self._ribbon_button(row, "网格线", self._toggle_mesh, True)
        row.addStretch()
        return page

    def _ribbon_views(self):
        page, row = self._ribbon_page()
        for label, key in (("等轴测", "ISO"), ("X 方向", "X"), ("Y 方向", "Y"), ("Z 方向", "Z")):
            self._ribbon_button(row, label, lambda checked=False, k=key: self.viewer.set_view(k))
        row.addStretch()
        return page

    def _separator(self):
        line = QFrame()
        line.setFrameShape(QFrame.VLine)
        line.setObjectName("separator")
        return line

    def _tree_panel(self):
        panel = QWidget()
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(12, 13, 8, 10)
        heading = QLabel("结果资源管理器")
        heading.setObjectName("panelHeading")
        layout.addWidget(heading)
        self.tree = QTreeWidget()
        self.tree.setHeaderHidden(True)
        task = QTreeWidgetItem(["▾  示例任务 001"])
        dataset = QTreeWidgetItem(["▾  合成流场 (3D)"])
        task.addChild(dataset)
        for name, (label, unit) in VARIABLES.items():
            item = QTreeWidgetItem([f"    {label}  [{unit}]"])
            item.setData(0, Qt.UserRole, name)
            dataset.addChild(item)
        views = QTreeWidgetItem(["▾  显示对象"])
        task.addChild(views)
        for mode in ("云图", "切面", "等值面", "流线"):
            item = QTreeWidgetItem([f"    {mode}"])
            item.setData(0, Qt.UserRole, mode)
            views.addChild(item)
        self.tree.addTopLevelItem(task)
        self.tree.expandAll()
        layout.addWidget(self.tree, 1)
        note = QLabel("内置样例 · 无外部结果文件")
        note.setObjectName("muted")
        layout.addWidget(note)
        return panel

    def _properties_panel(self):
        panel = QWidget()
        panel.setMinimumWidth(260)
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(12, 13, 14, 10)
        heading = QLabel("显示属性")
        heading.setObjectName("panelHeading")
        layout.addWidget(heading)
        group = QGroupBox("场变量")
        form = QFormLayout(group)
        self.variable_combo = QComboBox()
        for name, (label, unit) in VARIABLES.items():
            self.variable_combo.addItem(f"{label}  ({unit})", name)
        form.addRow("变量", self.variable_combo)
        self.palette_combo = QComboBox()
        self.palette_combo.addItems(["经典彩虹", "冷暖色", "灰度"])
        form.addRow("色谱", self.palette_combo)
        self.range_label = QLabel("自动范围")
        form.addRow("范围", self.range_label)
        layout.addWidget(group)

        self.slice_group = QGroupBox("切面参数")
        slice_form = QFormLayout(self.slice_group)
        self.axis_combo = QComboBox()
        self.axis_combo.addItems(["X", "Y", "Z"])
        self.axis_combo.setCurrentText("Z")
        slice_form.addRow("法向", self.axis_combo)
        self.slice_slider = QSlider(Qt.Horizontal)
        self.slice_slider.setRange(0, 100)
        self.slice_slider.setValue(50)
        slice_form.addRow("位置", self.slice_slider)
        self.slice_value = QLabel("50%")
        slice_form.addRow("区域", self.slice_value)
        layout.addWidget(self.slice_group)

        self.iso_group = QGroupBox("等值面参数")
        iso_form = QFormLayout(self.iso_group)
        self.iso_slider = QSlider(Qt.Horizontal)
        self.iso_slider.setRange(5, 95)
        self.iso_slider.setValue(55)
        iso_form.addRow("阈值", self.iso_slider)
        self.iso_value = QLabel("55%")
        iso_form.addRow("范围内", self.iso_value)
        layout.addWidget(self.iso_group)

        self.mesh_check = QCheckBox("叠加表面网格线")
        layout.addWidget(self.mesh_check)
        self.pick_info = QLabel("开启“拾取数值”后，在模型上单击。")
        self.pick_info.setObjectName("pickInfo")
        self.pick_info.setWordWrap(True)
        layout.addWidget(self.pick_info)
        layout.addStretch()
        self.slice_group.setVisible(True)
        self.iso_group.setVisible(False)
        self._update_range_label()
        return panel

    def _output_panel(self):
        tabs = QTabWidget()
        tabs.setObjectName("outputTabs")
        self.messages = QTextEdit()
        self.messages.setReadOnly(True)
        tabs.addTab(self.messages, "消息")
        table = QTextEdit()
        table.setReadOnly(True)
        table.setPlainText("示例任务 001   |   结果状态：已加载\n数据：合成结构网格   |   61 × 49 × 41 点\n可用变量：压力、温度、速度")
        tabs.addTab(table, "任务与数据")
        return tabs

    def _build_menu(self):
        file_menu = self.menuBar().addMenu("文件")
        shot = QAction("导出视图 PNG", self)
        shot.setShortcut(QKeySequence.SaveAs)
        shot.triggered.connect(self._export_png)
        file_menu.addAction(shot)
        data = QAction("导出当前几何 VTP", self)
        data.triggered.connect(self._export_vtp)
        file_menu.addAction(data)
        file_menu.addSeparator()
        file_menu.addAction("退出", self.close)
        view_menu = self.menuBar().addMenu("视图")
        view_menu.addAction("重置视角", lambda: self.viewer.set_view("ISO"))
        pick = QAction("拾取数值", self)
        pick.setShortcut("P")
        pick.triggered.connect(lambda: self.pick_button.click())
        view_menu.addAction(pick)

    def _connect(self):
        self.tree.itemClicked.connect(self._tree_clicked)
        self.variable_combo.currentIndexChanged.connect(self._variable_changed)
        self.palette_combo.currentTextChanged.connect(self._palette_changed)
        self.axis_combo.currentTextChanged.connect(self._slice_changed)
        self.slice_slider.valueChanged.connect(self._slice_changed)
        self.iso_slider.valueChanged.connect(self._iso_changed)
        self.mesh_check.toggled.connect(self._mesh_checked)
        self.viewer.pick_callback = self._show_pick

    def _tree_clicked(self, item, column):
        value = item.data(0, Qt.UserRole)
        if value in VARIABLES:
            self.variable_combo.setCurrentIndex(list(VARIABLES).index(value))
        elif value in self.mode_buttons:
            self._set_mode(value)

    def _set_mode(self, mode):
        self.viewer.mode = mode
        self.mode_buttons[mode].setChecked(True)
        self.slice_group.setVisible(mode == "切面")
        self.iso_group.setVisible(mode == "等值面")
        self.viewer.update_scene()
        self._update_status()
        self._log(f"显示模式：{mode}")

    def _variable_changed(self):
        self.viewer.variable = self.variable_combo.currentData()
        self.viewer.update_scene()
        self._update_range_label()
        self._update_status()

    def _palette_changed(self, palette):
        self.viewer.palette = palette
        self.viewer.update_scene()

    def _slice_changed(self, *_):
        self.viewer.axis = self.axis_combo.currentText()
        self.viewer.position = self.slice_slider.value() / 100
        self.slice_value.setText(f"{self.slice_slider.value()}%")
        if self.viewer.mode == "切面":
            self.viewer.update_scene()

    def _iso_changed(self, value):
        self.viewer.iso_fraction = value / 100
        self.iso_value.setText(f"{value}%")
        if self.viewer.mode == "等值面":
            self.viewer.update_scene()

    def _toggle_mesh(self, checked):
        self.mesh_check.setChecked(checked)

    def _mesh_checked(self, checked):
        self.mesh_button.setChecked(checked)
        self.viewer.show_mesh = checked
        self.viewer.update_scene()

    def _toggle_pick(self, checked):
        self.viewer.pick_enabled = checked
        self.pick_status.setText("拾取：开启" if checked else "拾取：关闭")
        self._log("拾取数值已开启。" if checked else "拾取数值已关闭。")

    def _show_pick(self, point, value):
        label, unit = VARIABLES[self.viewer.variable]
        message = f"X {point[0]:.3f}  Y {point[1]:.3f}  Z {point[2]:.3f}\n{label} {value:.3f} {unit}"
        self.pick_info.setText(message)
        self._log("拾取：" + message.replace("\n", "  |  "))

    def _update_range_label(self):
        name = self.variable_combo.currentData()
        low, high = self.viewer.data.GetPointData().GetArray(name).GetRange()
        self.range_label.setText(f"{low:.2f} — {high:.2f} {VARIABLES[name][1]}")

    def _update_status(self):
        label = VARIABLES[self.viewer.variable][0]
        self.status.setText(f"就绪  |  示例任务 001  |  {label}  |  {self.viewer.mode}")

    def _log(self, text):
        from PySide2.QtCore import QDateTime
        self.messages.append(f"[{QDateTime.currentDateTime().toString('HH:mm:ss')}]  {text}")

    def _export_png(self):
        path, _ = QFileDialog.getSaveFileName(self, "导出视图", "postprocess-view.png", "PNG 图像 (*.png)")
        if path:
            try:
                self.viewer.export_png(path)
                self._log(f"已导出视图：{path}")
            except Exception as exc:
                QMessageBox.critical(self, "导出失败", str(exc))

    def _export_vtp(self):
        path, _ = QFileDialog.getSaveFileName(self, "导出当前可见几何", "postprocess-geometry.vtp", "VTK PolyData (*.vtp)")
        if path:
            try:
                self.viewer.export_vtp(path)
                self._log(f"已导出当前几何：{path}")
            except Exception as exc:
                QMessageBox.critical(self, "导出失败", str(exc))


STYLE = """
QMainWindow, QWidget { background: #f6f8fb; color: #243246; font-family: 'PingFang SC'; font-size: 13px; }
QMenuBar { background: #e9eef5; padding: 3px; }
QMenuBar::item:selected, QMenu::item:selected { background: #dce8f8; }
#header { background: #1f3654; }
#header QLabel { color: #e8eef7; background: transparent; }
#header #title { font-size: 19px; font-weight: 700; }
#header #tag { color: #9ab2d0; font-size: 10px; margin-left: 12px; }
#ribbon { background: #edf2f8; border-bottom: 1px solid #d1dce9; }
#ribbon::pane { border: 0; background: #edf2f8; }
#ribbon QTabBar::tab { padding: 6px 18px; background: transparent; }
#ribbon QTabBar::tab:selected { color: #145caa; font-weight: 700; border-bottom: 2px solid #2472c4; }
QPushButton[ribbonButton="true"] { background: transparent; border: 1px solid transparent; border-radius: 5px; padding: 5px; }
QPushButton[ribbonButton="true"]:hover { background: #dde9f7; border-color: #c0d5ef; }
QPushButton[ribbonButton="true"]:checked { background: #d2e4f9; border-color: #73a9e6; color: #155aa4; font-weight: 700; }
#separator { color: #cbd7e5; margin: 7px 8px; }
#panelHeading { font-size: 14px; font-weight: 700; margin-bottom: 5px; }
#muted { color: #8897a8; font-size: 11px; }
#pickInfo { background: #eaf2fd; border: 1px solid #d1e2f8; border-radius: 5px; padding: 10px; color: #315a84; }
QTreeWidget, QTextEdit { background: #fff; border: 1px solid #d9e1eb; border-radius: 3px; }
QTreeWidget::item { padding: 5px 2px; }
QTreeWidget::item:selected { background: #d9eafa; color: #1b4e80; }
QGroupBox { background: #fff; border: 1px solid #d9e1eb; border-radius: 5px; margin-top: 10px; padding: 14px 10px 10px; font-weight: 600; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
QComboBox { background: #fff; border: 1px solid #cdd8e5; border-radius: 4px; padding: 5px; min-height: 23px; }
QSlider::groove:horizontal { height: 5px; background: #dce5ef; border-radius: 2px; }
QSlider::handle:horizontal { background: #2774bd; width: 15px; margin: -5px 0; border-radius: 7px; }
QStatusBar { background: #e7edf5; color: #40536a; }
#outputTabs::pane { border: 0; }
#outputTabs QTabBar::tab { padding: 7px 20px; }
"""


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("VTK 后处理窗口 Demo")
    app.setStyle("Fusion")
    app.setStyleSheet(STYLE)
    window = MainWindow()
    window.show()
    return app.exec_()


if __name__ == "__main__":
    raise SystemExit(main())
