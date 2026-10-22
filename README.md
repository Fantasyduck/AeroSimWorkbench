# AeroSimWorkbench

本仓库当前包含 A 负责的 C++/Qt 主工程骨架，以及此前的 [Python 后处理前端 Demo](demos/postprocess/README.md)。骨架窗口提供入口，通过独立进程打开 PySide2/VTK 演示窗口；这仍是前端原型，不是正式 C++ 后处理模块。

目录按职责统一：`app/` 放 GUI/CLI 入口与装配，`contracts/` 放公共契约，`modules/` 为 B～G 的正式模块交付区，`mocks/` 放当前占位实现，`demos/postprocess/` 保留独立后处理原型。`modules/` 目前只有交接说明；具体业务目标在各负责人交付后接入。详见 [目录与接入说明](docs/phase1-integration.md)。

## 快速开始

统一开发基线为 C++14、CMake 3.18.2、Qt 5.12.12、Windows VS2017 v141 x64 **Debug**。当前 B～G 正式模块尚未交付，普通骨架只查找 Qt Core/Widgets；目标机 VTK 8.2.0 仅提供 Debug 导出，因此完整 C++ 技术栈只在 Debug 配置下验证。团队配置和 Linux 模板见 [开发环境说明](docs/DevelopmentEnvironment.md)，协作边界见 [开发规范](docs/DevelopmentGuide.md)。

Windows（VS2017 开发者命令提示符）：

```bat
set "AEROSIM_QT_ROOT=C:\Qt\5.12.12\msvc2017_64"
scripts\build-windows.cmd all Debug
build-windows\Debug\aerosim_cli.exe --mock-once
build-windows\Debug\aerosim_gui.exe
```

Linux（需自行准备同版本 Qt；当前尚未实机验证）：

```bash
export AEROSIM_QT_ROOT=/path/to/qt-5.12.12
./scripts/build-linux.sh all
./build-linux/aerosim_cli --mock-once
```

无界面构建设置 `AEROSIM_BUILD_GUI=OFF`；仅编译某个当前模块时将 `all` 换为 `aerosim_contracts`、`aerosim_application`、`aerosim_mock`、`aerosim_ui_mock` 或 `aerosim_postprocess_demo_launcher`。CMake 3.18 不支持 Presets，因此统一使用脚本。VTK/SARibbon 只在显式启用 `AEROSIM_BUILD_STACK_PROBE=ON` 时查找；缺少它们不会阻断普通骨架构建。

## 模块边界

```text
GUI / CLI entry ──> ApplicationController ──> contracts <── Mock services
      │                      │                    ▲
      └── MockWindow ──> UI integration ports ────┘

正式接入：B GUI → G workflow/project services → C/D/F services → adapters
```

`ApplicationController` 只拥有服务对象并建立 G→F、F→G 的排队连接，不执行任何工作流判断或任务调度。GUI/CLI 都使用 `IWorkflowService` 和 `ITaskService`。跨线程消息按值传递，Qt 自定义类型在连接前注册；界面必须留在 GUI 线程。Mock 任务不会启动求解器，也不持久化记录。

运行 GUI 时点击 **Open post-processing VTK demo** 可打开独立的后处理窗口。需先在项目根目录的 `.venv-qt5` 安装 `demos/postprocess/requirements.txt`，并在目标 Windows 主机运行 `demos\postprocess\use_qt_5_12_12.cmd`；Windows 使用 `.venv-qt5\Scripts\pythonw.exe`，其他平台使用 `.venv-qt5/bin/python`。C++ 和 Python Demo 在目标机均运行 Qt 5.12.12，后者继续使用 VTK 9 演示数据。可用 `aerosim_gui --postprocess-smoke` 验证跨进程启动及 Demo 自测；该命令测试完会自动退出。

详细契约、目录结构、接入方法和未完成工作见 [第一阶段集成说明](docs/phase1-integration.md)；实际测试结果见 [构建与运行记录](docs/phase1-validation.md)。Windows 运行目录的打包与启动步骤见 [部署说明](docs/windows-deployment.md)。
