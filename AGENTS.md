# AeroSimWorkbench 开发约定

本文件适用于仓库内的 Codex 开发任务。只读取与当前任务有关的文档；详细设计、命令和历史验证以对应文档为准。当前仓库主要是 A 阶段 Qt 主工程、公共契约、Mock 和独立后处理 Demo；`modules/` 中的正式业务实现尚待成员交付。

## 架构与职责

- 保持 **UI Layer → Service Layer → Infrastructure & Adapter Layer**。UI 负责交互，Service 负责工程/工作流/业务，Infrastructure & Adapter 负责文件、求解器和运行环境。Service 不依赖 `QWidget` 或 VTK 显示控件；CLI 的公共服务应能在无界面环境使用。
- 总体七人分工：A 维护 `app/`、`contracts/`、顶层 CMake 和集成；B 维护 `modules/ui/`；C 维护 `modules/visualization/`；D 维护 `modules/configuration/`；E 维护 `modules/logging_ui/`；F 维护 `modules/runtime/`；G 维护 `modules/project/` 与 `modules/workflow/`。工程与工作流业务归 G，B 提供界面。A 的 `ApplicationController` 只装配对象、依赖和系统级连接，不承担工作流规则、参数校验或任务调度。
- 后处理专项 **A～E 是另一组五名成员**，与上述总体 A～G 不对应。其 16 项任务及建议接入位置见 `docs/PostProcessTaskHandoff.md`；不要据专项字母改变总体模块归属。

## 接口与实现边界

- 优先复用 `contracts/` 中的 `ResourceRef`、`MeshRef`、`ResultRef`、工作流/任务模型及访问端口。公共契约由总体 A 维护；跨模块改动应核对使用方、线程和生命周期，并与相关负责人评审。不要在模块内复制公共类型，也不要将大型 VTK 类型或网格数组放进 `contracts/`。
- 后处理文档中的网格/字段布局、场景操作、目标名称和功能端口仍是**待确认草案**；已加入代码的仅有轻量 `DatasetRef`、`SceneObjectRef`。未经相关成员确认，不要把草案写成公共接口、要求成员重构已有代码，或假设未交付的类与算法已经存在。
- 跨线程 Qt Signal/Slot 使用 `Qt::QueuedConnection`，按值传递轻量消息并预先注册自定义元类型；Widget 和 VTK 渲染操作留在 GUI 线程。明确对象所有权和退出清理，避免重复创建或重复连接。

## 构建、测试与占位代码

- 基线是 C++14、Qt 5.12.12、Windows VS2017 v141 x64 **Debug**、CMake 3.18.2；正式 VTK C++ 为 8.2.0，目标机现有包仅有 Debug 导出；SARibbon 开发基线为 2.9.3。nlohmann/json 版本及日志库选择尚未定案。不要擅自升级、混用 ABI/32 位库或把个人绝对路径写入 CMake。依赖由实际使用它的目标声明，不阻断无关模块。
- 模块先提供可独立构建的目标和必要的自测，再接入顶层构建。Windows 使用 `scripts/build-windows.cmd <target|all> Debug`，全量构建后运行 CTest；无界面构建可设 `AEROSIM_BUILD_GUI=OFF`。缺少匹配依赖时如实报告，不能宣称构建通过；Linux/macOS 全栈与 Windows VTK Release 尚未验证。
- `mocks/` 只用于未交付服务的装配和异步通信验证；`demos/postprocess/` 是独立 Python/PySide2/VTK 9 原型，不是正式 C++/VTK 8.2 实现。不要把 Mock/Demo 结果写成真实求解、数据读取或后处理验收。保留现有可用代码和 Demo，按任务范围修改。

## 按任务选择文档

不要每次通读整个 `docs/`。先读与改动对应的条目，涉及其他边界时再补读：

| 任务 | 必读文档 |
| --- | --- |
| 通用代码风格、分支/PR、模块职责 | `docs/DevelopmentGuide.md`；涉及模块时再读其 `modules/<module>/README.md` |
| 工具链、依赖、CMake、独立构建 | `docs/DevelopmentEnvironment.md`；核查既往实测才读 `docs/phase1-validation.md` |
| `app/`、`contracts/`、Mock、GUI/CLI 装配 | `docs/phase1-integration.md`；公共契约改动再读 `docs/DevelopmentGuide.md` |
| 后处理数据、场景、专项代码接入 | `docs/PostProcessIntegrationGuide.md`、`docs/PostProcessContracts.md`；任务分配读 `docs/PostProcessTaskHandoff.md`，夹具/验收读 `docs/PostProcessTestData.md` |
| Python 后处理 Demo | `demos/postprocess/README.md`；与正式模块交接时再读后处理集成文档 |
| Windows 打包或目标机部署 | `docs/windows-deployment.md`，并核对 `docs/DevelopmentEnvironment.md` 的 Debug/Release 适用范围 |

遇到文档冲突时，先核对代码和较新的实测记录，区分历史记录与当前规范；不要自行更改已确定的成员职责或将评审草案视为定案。
