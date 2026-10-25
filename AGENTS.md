# AeroSimWorkbench 开发约定

本文件适用于仓库内的 Codex 开发任务。按任务选择文档，不必每次通读 `docs/`；文档导航见 `docs/README.md`。当前仓库包含第一阶段 Qt 主工程、公共契约和 Mock；`modules/visualization/` 另有一个可独立构建的空间特征目标。

## 架构与模块边界

- 保持 **UI Layer → Service Layer → Infrastructure & Adapter Layer**。`modules/ui/` 提供窗口与交互；`modules/visualization/` 提供 VTK 视图、网格/结果与空间后处理；`modules/configuration/` 提供参数和工况；`modules/logging_ui/` 提供日志/错误控件；`modules/runtime/` 提供任务运行与日志后端；`modules/project/`、`modules/workflow/` 提供工程与工作流服务。Service 不依赖 `QWidget` 或 VTK 显示控件；CLI 的公共服务应能在无界面环境使用。
- `app/ApplicationController` 只装配应用对象、依赖和系统级连接，不承担工作流规则、参数校验或任务调度。主窗口通过 `IWidgetHost` 接入视口和日志控件；工作流/工程服务提供业务状态，窗口只展示和发出操作请求。

## 接口与实现边界

- 优先复用 `contracts/` 中的工程、网格、结果引用和工作流/任务端口。修改跨模块契约时核对调用方、兼容性、线程和生命周期；不要在模块内复制公共类型，也不要将大型 VTK 类型或网格数组放进 `contracts/`。
- 后处理文档中的网格/字段布局、场景操作、建议 CMake 目标和功能端口仍是**待评审草案**；已加入代码的只有轻量 `DatasetRef`、`SceneObjectRef`。未经接口评审，不把草案写成已确定公共接口，也不要求现有模块迁移到未确认设计。
- 跨线程 Qt Signal/Slot 使用 `Qt::QueuedConnection`，按值传递轻量消息并预先注册自定义元类型；Widget 和 VTK 渲染操作留在 GUI 线程。明确对象所有权和退出清理，避免重复创建或重复连接。

## 构建、测试与占位代码

- 已验证的开发基线为 C++14、Qt 5.12.12、Windows VS2017 v141 x64 **Debug**、CMake 3.18.2；正式 VTK C++ 版本为 8.2.0，现有安装仅提供 Debug 导出；SARibbon 开发基线为 2.9.3。nlohmann/json 版本及日志库选择尚未定案。不要擅自升级、混用 ABI/32 位库或将个人绝对路径写入 CMake；依赖由实际使用它的目标声明。
- 正式模块应能独立构建和自测，再接入顶层构建。Windows 使用 `scripts/build-windows.cmd <target|all> Debug`，全量构建后运行 CTest；无界面构建可设 `AEROSIM_BUILD_GUI=OFF`。缺少匹配依赖时如实报告；Linux/macOS 全栈与 Windows VTK Release 尚未验证。
- `mocks/` 只验证装配和异步消息，不代表真实求解、数据读取或后处理验收。正式后处理只采用 C++、Qt 5.12.12 和 VTK 8.2.0；保留已有公共接口及与后处理原型无关的通用脚本执行能力。

## 按任务选择文档

| 任务 | 参考文档 |
| --- | --- |
| 文档总览 | `docs/README.md` |
| 通用代码风格、PR、模块结构 | `docs/DevelopmentGuide.md`；具体模块再读其 `modules/<module>/README.md` |
| 工具链、依赖、CMake、独立构建 | `docs/DevelopmentEnvironment.md`；核查既往实测才读 `docs/phase1-validation.md` |
| `app/`、`contracts/`、Mock、GUI/CLI 装配 | `docs/phase1-integration.md`；契约改动再读 `docs/DevelopmentGuide.md` |
| 后处理数据、场景、功能接入 | `docs/PostProcessIntegrationGuide.md`、`docs/PostProcessContracts.md`；功能依赖读 `docs/PostProcessTaskHandoff.md`，夹具读 `docs/PostProcessTestData.md` |
| Windows 打包或目标机部署 | `docs/windows-deployment.md`，并核对 `docs/DevelopmentEnvironment.md` 的 Debug/Release 适用范围 |

遇到文档冲突时，先核对代码和较新的实测记录，区分历史记录与当前规范；不要将评审草案视为定案。
