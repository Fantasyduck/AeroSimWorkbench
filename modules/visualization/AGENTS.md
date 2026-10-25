# 可视化与后处理模块约定

本文件适用于 `modules/visualization/` 及其子目录；同时遵守仓库根目录的 `AGENTS.md`。开始修改前阅读本目录的 `README.md`，涉及跨模块数据、场景或工作区时再阅读 `docs/PostProcessIntegrationGuide.md` 和 `docs/PostProcessContracts.md`。

## 分层与目录

- `data/` 负责格式读取、网格/字段规范化和帧索引；不得创建 QWidget、renderer 或 actor。
- `features/` 负责从只读数据快照计算切面、等值线/面、流线、迹线和可导出的特征几何；不得持有场景状态或操作渲染器。
- `scene/` 是唯一可以创建 Viewer、VTK renderer、mapper 和 actor 的位置；所有场景变更和 `Render()` 留在 GUI 线程。
- `analysis/` 负责多工况表、公式、曲线和模型，读取数据/工况元信息但不依赖 `scene/`、QWidget 或主窗口。
- 后处理工作区和参数控件属于 `modules/ui/`，它也是前处理、工程管理和运行监控等系统界面的统一位置；`app/` 只装配对象。不要把 UI 代码放入本模块的 data/features/analysis。

依赖方向为 `data -> features -> scene`；`analysis` 可依赖数据元信息但不依赖 scene。不能从低层反向包含 UI 或 scene 头文件。当前只有 `features/` 已实现；创建其他层前先完成其最小 API、测试数据和独立 CMake 目标。

## 数据、线程与公共契约

- `contracts/` 只使用或补充按值传递的引用与元信息。不得写入 `vtkDataObject`、`vtkRenderer`、数组缓存、裸 VTK 指针或模块私有几何类型。
- 特征算法应接收调用方提供的只读输入快照，返回独立、不可共享写入的几何结果；输入、字段关联、单位和坐标系必须显式。
- 文件 IO、网格规范化和特征计算可在工作线程执行。QWidget、render window、renderer、actor 和 `Render()` 只能在 GUI 线程执行。跨线程 Qt 消息仅传轻量、已注册的值类型；不传裸 VTK 指针。
- 每项耗时操作都要说明取消、数据版本失效和结果所有权。读取/计算失败不得创建半成品场景对象。

## 构建与测试

- 固定 C++14、Qt 5.12.12、VTK 8.2.0；目标 Windows 环境仅有 VTK Debug，不得让 VTK 目标链接 Release。
- 模块目标自行声明所需 Qt/VTK/格式库依赖。纯数据索引或分析代码不应强制链接 VTK、Widgets 或 SARibbon。
- 每个新目标都要有独立构建与有意义的输入/输出测试。现有空间特征测试按 `tests/README.md` 运行；不要把 Mock 测试说成真实算法验证。
