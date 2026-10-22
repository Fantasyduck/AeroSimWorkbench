# 第一阶段主工程与集成说明

本阶段只验证 A 的主工程、公共契约和异步装配。依据《框架设计与技术调研.统一修改版》中的三层架构、七人分工及异步通信方案，保留了既有 Python Demo。会议记录中的实际求解、网格、参数、监控等需求由后续模块承担。

## 目录

```text
.
├── CMakeLists.txt
├── app/                       # A：GUI/CLI 入口与 ApplicationController
├── contracts/                 # A：Qt Core 契约与 UI 插入契约
│   ├── include/aerosim/contracts/
│   ├── include/aerosim/ui/IntegrationPorts.h
│   └── src/RegisterMetaTypes.cpp
├── modules/                   # B～G：正式模块，当前仅有交接说明
│   ├── ui/                    # B
│   ├── visualization/         # C
│   ├── configuration/         # D
│   ├── logging_ui/            # E
│   ├── runtime/               # F
│   ├── project/               # G
│   └── workflow/              # G
├── mocks/services/            # G/F 服务的临时实现
├── mocks/ui/                  # B 主窗口的临时实现
├── demos/postprocess/         # 独立 Python/VTK Demo 与 C++ 启动适配
├── cmake/stack_probe/         # 可选 Qt/VTK/SARibbon 兼容性探针
├── scripts/                   # 统一构建脚本
└── docs/                      # 开发、集成和验证说明
```

`aerosim_contracts` 只依赖 Qt Core；UI 插入契约只有 `QWidget` 前置声明，不使 CLI 链接 Widgets。`aerosim_application` 和 `aerosim_mock` 也只依赖 Qt Core；`aerosim_ui_mock` 依赖 Qt Widgets。CLI 不链接 Qt Widgets。`modules/` 中的正式目标待各负责人交付后由 A 接入，当前不能宣称已经构建。

Python/PySide2/VTK 后处理窗口由 `PostprocessDemoLauncher` 在独立进程中启动；MockWindow 只发出用户操作信号，`app/gui_main.cpp` 完成系统级连接。启动器从可执行文件目录向上寻找项目根目录，可用 `AEROSIM_PROJECT_ROOT` 指定根目录；并使用项目 `.venv-qt5` 中的 Python。主窗口退出时会结束其启动的子进程。该适配只服务现有 UI 演示，不取代 C/B 的正式 VTK/SARibbon 模块。

## 公共契约

- `References.h`：工程、资源、作用域、参数快照、网格和结果均用轻量标识或路径引用传递。大网格与流场数组不跨信号槽复制。
- `WorkflowModels.h`：`ValidationResult`、`WorkflowView`、步骤状态、可用操作、导航标识及视图版本。G 实现规则、失效和恢复；UI 只读取视图。
- `DataAccess.h`：工程、参数、网格及结果的最小只读访问边界。G/C/D 的具体实现和错误策略由各负责人完善。
- `TaskModels.h`：命令、事件、显式状态/进度字段以及 F 持有的 `TaskRecord` 形状。命令被投递不等于执行成功；Mock 的完成事件仅说明消息闭环。
- `Telemetry.h`：日志、错误和监控样本的公共消息字段，供 F/E/B 对接。
- `TaskPorts.h`：G 的 `IWorkflowService` 与 F 的 `ITaskService` 信号槽端口。A 只建立一次 `Qt::QueuedConnection`，不解释业务事件。
- `IntegrationPorts.h`：B 的主窗口实现 `IWidgetHost`，使用逻辑 `WidgetSlot` 接入 C/E 控件，并通过 `ServiceBindings` 接收服务。`insertWidget` 成功后 QWidget 由窗口的 Qt 父对象管理；失败时仍由调用者释放。

服务对象在入口线程创建和销毁。当前 Mock 服务与 GUI 同属主线程，队列消息由主线程事件循环处理；F 可在自己的服务内部使用 QProcess 或工作线程，不应在槽中阻塞。正式异步实现的持久化、去重、过期事件和线程数据同步由 F/G 负责。`ApplicationController::initialize()` 可重复调用而不增加连接，`shutdown()` 断开连接。一个控制器只拥有每项服务的一个实例。

## B～G 的接入

| 成员 | 接入点与本阶段未完成工作 |
| --- | --- |
| B | 以真正的 `SARibbonMainWindow` 替换 `MockWindow`，实现 `IWidgetHost`，展示 G 的 `WorkflowView`；提供独立 CMake 目标。 |
| C | 实现 `IMeshAccess`/`IResultAccess`、VTK 8.2 viewer 和数据/后处理模块；通过 B 的 `WidgetSlot::Viewport` 插入。 |
| D | 实现 `IParameterAccess`、配置与工况模型、专业校验及 JSON；保持服务层可在 CLI 使用。 |
| E | 实现日志与错误 QWidget，通过 `WidgetSlot::Log/Error` 插入，消费公共 `LogEntry`/`ErrorInfo`。 |
| F | 实现 `ITaskService`、TaskRecord 可靠保存、去重、求解器进程/适配器和日志后端；保留 `taskEventOccurred` 契约。真实 Shell 命令解析与执行也归 F，当前 CLI 只是入口示范。 |
| G | 实现 `IWorkflowService`、ProjectManager、工作流校验/恢复；只有 G 在规则通过后发出 TaskCommand。 |

A 在各模块提供构建目标后，把它们传给 `ApplicationController`，替换 `mocks/services/` 的两个对象；GUI 入口换成 B 的窗口，并将 C/E widget 插入。目标依赖保持 UI→Service→Infrastructure，服务不链接 Qt Widgets 或 VTK 窗口。系统级 G→F、F→G 连接只由 A 在控制器建立一次，模块内部连接由模块负责人负责。

设计文档后部的“模块归属与交接表”将 `WorkflowManager` 和 `ProjectManager` 写为 B，与前面的七人分工、G 专节以及本阶段任务描述冲突。本说明按明确的七人分工采用 **G 负责工程与工作流，B 负责界面**；正式接口评审时需统一文档。

## 验收边界

`aerosim_cli --mock-once` 输出排队命令、Prepared/Finished 事件，并检查处理没有在提交调用栈内同步发生。`aerosim_gui --smoke` 创建占位窗口并跑同一消息闭环。两者都不代表参数有效、任务真正执行或结果可用。真实 VTK、SARibbon、求解器、MPI/HPC 和完整工作流不在本阶段。
