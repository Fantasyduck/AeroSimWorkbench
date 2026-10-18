# 后处理窗口 Demo

这是一个独立运行的 Qt + VTK 原型。界面参考 Tecplot 360 的工作方式，将结果树、三维视图、显示属性和输出区放在同一窗口。启动时生成一组**合成流场**，无需项目求解器或真实结果文件。

C++ 主工程的 Mock 窗口现提供“Open post-processing VTK demo”入口，调用本目录的 `main.py` 打开独立进程窗口。两个 Qt 版本不在同一进程中加载；正式 C++/VTK 模块仍由 C/B 后续接入。

## 运行

建议使用 Python 3.11 或更新版本。在项目根目录执行：

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r postprocess_demo/requirements.txt
.venv/bin/python postprocess_demo/main.py
```

鼠标左键旋转、中键平移、滚轮缩放；`P` 键或工具栏的“拾取”开启点值拾取。变量、云图/切面/等值面/流线、切面方向与位置、等值面阈值、色谱、网格线和视角均可交互。可导出 PNG 截图及当前可见几何的 VTP 文件。

## 范围

- 以 `vtkImageData` 生成内置压力、温度和速度场，演示 VTK 渲染和空间后处理链路。
- “示例任务 001”是演示数据，不做真实结果文件和任务/网格匹配校验。
- MSH/CAS/CGNS/求解器结果读取、Tecplot/CGNS/Plot3D 转换、时间序列、二维残差曲线和真实工作流服务留给正式模块；界面内没有把这些功能伪装成已实现。
- 正式方案文档指定 Qt 5.12 + VTK 8.2 + SARibbon。此原型为方便在当前环境运行，使用 PySide6 + VTK 9 的 Python 绑定；窗口和渲染职责可作为后续 C++/Qt5 实现的参考。
