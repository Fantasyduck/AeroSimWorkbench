# 工程开发规范

本规范依据 UI、Service、Infrastructure & Adapter 三层架构制定。第一阶段仍有 Mock；它必须标明占位性质，不得当作正式业务模块。正式后处理采用 C++/Qt 5.12.12/VTK 8.2。

## 目录、目标与职责

| 目录或目标 | 技术职责 |
| --- | --- |
| 根 `CMakeLists.txt`、`cmake/`、`scripts/`、`contracts/`、`app/` | 统一构建、公共契约、应用装配、系统级异步连接和集成验收 |
| `contracts/include/aerosim/ui/` | Widget 插入与服务绑定契约 |
| `mocks/ui/`、`mocks/services/` | 正式模块接入前的明确占位目标 |
| `tests/fixtures/postprocess/` | 小型解析网格、场、时序和多工况测试数据；以 CMake 脚本检查完整性 |
| `modules/ui/` | SARibbon 主窗口、WorkflowPanel、任务/监控/曲线 UI |
| `modules/visualization/` | VTK 8.2 viewer、网格/结果读取与空间后处理 |
| `modules/configuration/` | 参数、工况、输入快照、JSON 与专业校验 |
| `modules/logging_ui/` | 日志与错误显示控件 |
| `modules/runtime/` | SolverManager、TaskRecord、运行适配器、日志后端 |
| `modules/project/`、`modules/workflow/` | ProjectManager、WorkflowManager 与工程关联 |

`modules/visualization/` 已有正式的 `aerosim_spatial_features` 目标；其余模块尚未交付业务目标。新模块使用独立子目录、`CMakeLists.txt`、库目标和测试目标；先独立构建和自测，再接入根 CMake。UI 依赖公共契约与 Service；Service 不依赖 `QWidget`、主窗口或 VTK 显示控件；Infrastructure 提供文件、进程、数据等适配。CLI 只要求 Qt Core。

`modules/project/` 和 `modules/workflow/` 提供工程与工作流业务服务；`modules/ui/` 展示状态并提交操作请求，不重复业务规则。

## Git 与 Pull Request

- 从最新 `main` 创建短分支，建议 `feature/ui-ribbon-window`、`feature/visualization-vtk-viewer`、`fix/runtime-task-exit`、`docs/build-guide`；不要在共享目标机直接开发业务代码。
- 一次提交聚焦一个可解释的变更，提交信息写清模块与动作，例如 `visualization: add VTK viewer target`。不提交 `build-*`、`.venv-*`、本机 CMake 缓存、日志或个人路径。
- PR 说明行为变化、受影响的公共接口/依赖、独立构建命令、测试结果及未验证平台。UI 变更附必要的界面说明；有二进制/许可证影响的依赖变更单独写明。
- 模块 PR 先给出独立构建与自测结果，再进行跨模块集成；合并后运行主工程测试。

## C++ 与文件组织

- 统一 C++14、Qt 5.12.12、MSVC v141 x64 Debug 开发基线。公共头文件放在 `contracts/include/aerosim/`；模块公开头文件放在本模块的 `include/aerosim/<module>/`，实现放在模块 `src/`。骨架代码已按职责迁入 `app/`、`mocks/` 和 `demos/`；后续目录调整单独提交。
- 类型名用 `PascalCase`，函数/局部变量用 `camelCase`，成员变量用尾随下划线（如 `taskService_`），常量用清晰的 `kPascalCase` 或项目已有命名。接口类型沿用现有 `I...` 契约；不要为未交付业务预先堆叠抽象。
- 一个主要类对应一个 `.h`/`.cpp`；头文件只包含必要依赖，优先前置声明。实现文件包含所需头，不依赖间接包含。Qt `QObject` 类需由 CMake `AUTOMOC` 处理。
- 脚本换行由根目录 `.gitattributes` 固定：`.sh` 为 LF，Windows `.cmd/.bat` 为 CRLF；不要因编辑器自动转换而提交整文件换行差异。
- 缩进 4 空格；遵循相邻文件现有格式。新增目标使用 `target_*` 声明 include、宏和链接依赖，不使用全局 `include_directories`/`link_directories`。Windows MSVC 源码按 UTF-8 编译。
- 明确对象所有权和线程归属；跨线程 Signal/Slot 用 `Qt::QueuedConnection`，按值传递并注册自定义类型。界面对象只在 GUI 线程使用，长时间运行的求解器通过运行模块的异步执行端处理。

## 公共契约与第三方依赖

`contracts/` 中的 Core 与 UI 插入接口是跨模块公共契约。需要改动时，在 PR 中说明调用方、数据生命周期、线程语义与兼容影响；完成接口评审后再修改契约和 Mock，最后更新实现。不得在模块内复制一份相似的 TaskCommand、TaskEvent、WorkflowView 等公共类型。

第三方依赖由使用模块单独声明。新增或变更前记录准确版本、来源、许可证、编译器/架构、Debug/Release 包、CMake 包名及集成测试结果，并检查根构建影响。禁止提交个人绝对路径、静默下载未知版本或替换已确定的正式 Qt/VTK 版本。目标机现有 VTK 8.2.0 仅有 Debug 导出，禁止将其链接到 Release 模块。nlohmann/json 版本和日志库方案未定前，不得擅自升级/替换技术选型。可用 `Qt5_DIR`、`VTK_DIR`、`SARIBBON_SOURCE_DIR` 等配置变量传入本机位置。

## 独立构建与 Mock

每个模块 PR 至少包含自己的库目标、所需公开 include/链接依赖、一个能说明真实接口行为的最小测试或可运行演示，以及缺失可选依赖时的构建说明。使用 `scripts/build-windows.cmd <target> Debug` 或 `scripts/build-linux.sh <target>` 只构建本模块目标。仅依赖 Qt Core 的服务不得强制查找 VTK/SARibbon；VTK 目标或 SARibbon 目标应由各自 CMake 子目录声明并允许关闭。

尚未实现的相邻服务使用公共接口注入 Mock。Mock 类、窗口标题、日志和测试名称都应明确体现占位性质；测试只宣称验证装配或通信，不宣称真实求解、网格算法和结果正确性。Widget 的创建、插入失败清理和所属 GUI 线程按公共接口约定处理；工作流服务与运行服务的状态职责保持分离。正式模块替换 Mock 后复查依赖方向、单实例创建、只连接一次信号及退出清理。

环境的具体版本、安装方式与命令见[开发环境说明](DevelopmentEnvironment.md)。
