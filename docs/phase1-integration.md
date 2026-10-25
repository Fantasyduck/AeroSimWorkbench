# 第一阶段主工程与集成说明

本阶段只验证主工程、公共契约和异步装配。项目保持 UI、Service、Infrastructure & Adapter 三层架构。真实求解、网格、参数和监控能力尚未接入；本页记录当前骨架的技术边界，不代替正式模块设计。正式后处理采用 C++、Qt 5.12.12 和 VTK 8.2.0。

## 目录

```text
.
├── CMakeLists.txt
├── app/                       # GUI/CLI 入口与 ApplicationController
├── contracts/                 # Qt Core 契约与 UI 插入契约
│   ├── include/aerosim/contracts/
│   ├── include/aerosim/ui/IntegrationPorts.h
│   └── src/RegisterMetaTypes.cpp
├── modules/                   # 正式模块预留区，当前仅有模块说明
│   ├── ui/                    # 主窗口与界面
│   ├── visualization/         # VTK 与网格/结果
│   ├── configuration/         # 参数与工况
│   ├── logging_ui/            # 日志与错误控件
│   ├── runtime/               # 任务运行与适配
│   ├── project/               # 工程管理
│   └── workflow/              # 工作流管理
├── mocks/services/            # 工作流/任务服务的临时实现
├── mocks/ui/                  # 主窗口的临时实现
├── tests/fixtures/postprocess/ # 后处理解析测试数据及 CMake 检查
├── cmake/stack_probe/         # 可选 Qt/VTK/SARibbon 兼容性探针
├── scripts/                   # 统一构建脚本
└── docs/                      # 开发、集成和验证说明
```

`aerosim_contracts` 只依赖 Qt Core；UI 插入契约只有 `QWidget` 前置声明，不使 CLI 链接 Widgets。`aerosim_application` 和 `aerosim_mock` 也只依赖 Qt Core；`aerosim_ui_mock` 依赖 Qt Widgets。CLI 不链接 Qt Widgets。顶层 CMake 当前只接入骨架、Mock、数据夹具检查和可选技术栈探针；`modules/` 中没有正式业务目标。

MockWindow 只提供任务通信按钮和占位视口。正式后处理视口拟通过主窗口的 `IWidgetHost` 接入；具体场景接口仍待评审。主工程当前不会启动后处理子进程。

## 公共契约

- `References.h`：工程、资源、作用域、参数快照、网格和结果均用轻量标识或路径引用传递。大网格与流场数组不跨信号槽复制。
- `WorkflowModels.h`：`ValidationResult`、`WorkflowView`、步骤状态、可用操作、导航标识及视图版本。工作流服务处理规则、失效和恢复；UI 只读取视图。
- `DataAccess.h`：工程、参数、网格及结果的最小只读访问边界。具体业务实现和错误策略随对应模块接入。
- `TaskModels.h`：命令、事件、显式状态/进度字段以及运行模块保存的 `TaskRecord` 形状。命令被投递不等于执行成功；Mock 的完成事件仅说明消息闭环。
- `Telemetry.h`：日志、错误和监控样本的公共消息字段，供运行、日志和 UI 模块对接。
- `TaskPorts.h`：`IWorkflowService` 与 `ITaskService` 信号槽端口。应用装配层只建立一次系统级 `Qt::QueuedConnection`，不解释业务事件。
- `IntegrationPorts.h`：正式主窗口需实现 `IWidgetHost`，使用逻辑 `WidgetSlot` 接入视口与日志控件，并通过 `ServiceBindings` 接收服务。`insertWidget` 成功后 QWidget 由窗口的 Qt 父对象管理；失败时仍由调用者释放。

服务对象在入口线程创建和销毁。当前 Mock 服务与 GUI 同属主线程，队列消息由主线程事件循环处理；运行服务可在内部使用 QProcess 或工作线程，不应在槽中阻塞。正式异步实现需分别处理任务事实的持久化/去重以及工作流过期事件/线程数据同步。`ApplicationController::initialize()` 可重复调用而不增加连接，`shutdown()` 断开连接。一个控制器只拥有每项服务的一个实例。

## 正式模块接入

| 模块 | 接入点与当前未实现能力 |
| --- | --- |
| `modules/ui/` | 以正式 `SARibbonMainWindow` 替换 `MockWindow`，实现 `IWidgetHost`，展示 `WorkflowView`；提供独立 CMake 目标。 |
| `modules/visualization/` | 实现 `IMeshAccess`/`IResultAccess`、VTK 8.2 viewer 和数据/后处理模块；通过 `WidgetSlot::Viewport` 插入主窗口。 |
| `modules/configuration/` | 实现 `IParameterAccess`、配置与工况模型、专业校验及 JSON；保持服务层可在 CLI 使用。 |
| `modules/logging_ui/` | 实现日志与错误 QWidget，通过 `WidgetSlot::Log/Error` 插入，消费公共 `LogEntry`/`ErrorInfo`。 |
| `modules/runtime/` | 实现 `ITaskService`、TaskRecord 可靠保存、去重、求解器进程/适配器和日志后端；保留 `taskEventOccurred` 契约。真实 Shell 命令解析与执行也在本模块，当前 CLI 只是入口示范。 |
| `modules/project/`、`modules/workflow/` | 实现 `IWorkflowService`、ProjectManager、工作流校验/恢复；工作流规则通过后才发出 TaskCommand。 |

正式模块提供构建目标后，将其接入 `ApplicationController`，替换 `mocks/services/` 的两个对象；GUI 入口换成正式主窗口，再插入视口和日志控件。目标依赖保持 UI→Service→Infrastructure，服务不链接 Qt Widgets 或 VTK 窗口。工作流与任务服务之间的系统级连接只在控制器建立一次；模块内部连接由模块自身建立。

## 验收边界

`aerosim_cli --mock-once` 输出排队命令、Prepared/Finished 事件，并检查处理没有在提交调用栈内同步发生。`aerosim_gui --smoke` 创建占位窗口并跑同一消息闭环。两者都不代表参数有效、任务真正执行或结果可用。真实 VTK、SARibbon、求解器、MPI/HPC 和完整工作流不在本阶段。
