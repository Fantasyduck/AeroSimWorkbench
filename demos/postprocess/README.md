# 后处理窗口 Demo

这是一个独立运行的 Qt + VTK 原型。界面参考 Tecplot 360 的工作方式，将结果树、三维视图、显示属性和输出区放在同一窗口。启动时生成一组**合成流场**，无需项目求解器或真实结果文件。

C++ 主工程的 Mock 窗口提供“Open post-processing VTK demo”入口，调用本目录的 `main.py` 打开独立进程窗口。目标 Windows 主机两端统一使用 Qt 5.12.12；正式 C++/VTK 模块仍由 C/B 后续接入。

## 运行

Windows 目标机使用 Python 3.6 x64；PySide2 5.12.6 的 Windows 预编译包支持 Python 3.5～3.7。在项目根目录执行：

```bat
"C:\Program Files (x86)\Microsoft Visual Studio\Shared\Python36_64\python.exe" -m venv .venv-qt5
.venv-qt5\Scripts\python.exe -m pip install -r demos\postprocess\requirements.txt
demos\postprocess\use_qt_5_12_12.cmd
.venv-qt5\Scripts\python.exe demos\postprocess\smoke.py
.venv-qt5\Scripts\pythonw.exe demos\postprocess\main.py
```

PySide2 5.12.6 的预编译包自带 Qt 5.12.6 DLL。`use_qt_5_12_12.cmd` 把虚拟环境中的 Qt Core/Gui/Widgets/OpenGL DLL 和 Windows 平台插件替换为本机 Qt 5.12.12 SDK 的版本，同时在虚拟环境内备份原文件；它不会修改 C++ SDK。脚本要求 SDK 位于 `C:\Qt\5.12.12\msvc2017_64`。后处理自测会断言运行时版本为 5.12.12。

鼠标左键旋转、中键平移、滚轮缩放；`P` 键或工具栏的“拾取”开启点值拾取。变量、云图/切面/等值面/流线、切面方向与位置、等值面阈值、色谱、网格线和视角均可交互。可导出 PNG 截图及当前可见几何的 VTP 文件。

## 范围

- 以 `vtkImageData` 生成内置压力、温度和速度场，演示 VTK 渲染和空间后处理链路。
- “示例任务 001”是演示数据，不做真实结果文件和任务/网格匹配校验。
- MSH/CAS/CGNS/求解器结果读取、Tecplot/CGNS/Plot3D 转换、时间序列、二维残差曲线和真实工作流服务留给正式模块；界面内没有把这些功能伪装成已实现。
- 正式方案文档指定 Qt 5.12 + VTK 8.2 + SARibbon。此原型使用 PySide2 5.12.6 绑定和 Qt 5.12.12 运行时，以及 VTK 9.3 的 Python 绑定；VTK 8.2 C++ 模块和 SARibbon 仍由对应负责人接入。
