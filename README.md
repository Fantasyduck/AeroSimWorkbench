# AeroSimWorkbench

本仓库当前包含 A 负责的 C++/Qt 主工程骨架，以及此前的 [Python 后处理前端 Demo](postprocess_demo/README.md)。骨架窗口提供入口，通过独立进程打开原有 PySide6/VTK 演示窗口；这仍是前端原型，不是正式 C++ 后处理模块。

## 第一阶段运行

骨架最低构建依赖为 CMake 3.16、C++14 编译器、Qt 5.12+ 的 Core 和 Widgets。架构目标版本为 Qt 5.12.12、VTK 8.2、SARibbon；本阶段没有 B/C 的实现，因此不链接 VTK 或 SARibbon，也不将占位界面称为正式主窗口。正式接入时应统一并验证这些版本。

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/5.12.12
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/aerosim_cli --mock-once
./build/aerosim_gui
```

在纯命令行环境可以关闭 GUI，服务层无需 Qt Widgets：

```sh
cmake -S . -B build-cli -DAEROSIM_BUILD_GUI=OFF -DCMAKE_PREFIX_PATH=/path/to/Qt/5.12.12
cmake --build build-cli --parallel
./build-cli/aerosim_cli --mock-once
```

Windows 上将可执行文件路径改为 `build\Release\aerosim_cli.exe` 等实际构建配置路径。CMake 3.18 的 CTest 需先进入 `build` 目录，再运行 `ctest -C Release --output-on-failure`。找不到 Qt 时，设置 `CMAKE_PREFIX_PATH` 到 Qt 安装根目录，或设置 `Qt5_DIR` 到 `lib/cmake/Qt5`。VTK 8.2 和 SARibbon 的查找与链接由 C/B 模块提交时加入其独立目标；它们缺失不会被误报为本阶段的成功验证。

## 模块边界

```text
GUI / CLI entry ──> ApplicationController ──> contracts <── Mock services
      │                      │                    ▲
      └── MockWindow ──> UI integration ports ────┘

正式接入：B GUI → G workflow/project services → C/D/F services → adapters
```

`ApplicationController` 只拥有服务对象并建立 G→F、F→G 的排队连接，不执行任何工作流判断或任务调度。GUI/CLI 都使用 `IWorkflowService` 和 `ITaskService`。跨线程消息按值传递，Qt 自定义类型在连接前注册；界面必须留在 GUI 线程。Mock 任务不会启动求解器，也不持久化记录。

运行 GUI 时点击 **Open post-processing VTK demo** 可打开独立的后处理窗口。需先在项目根目录的 `.venv` 安装 `postprocess_demo/requirements.txt`；Windows 使用 `.venv\Scripts\pythonw.exe`，其他平台使用 `.venv/bin/python`。C++ 的 Qt 5 与 Python Demo 的 Qt 6/VTK 9 分属不同进程，不会混装。可用 `aerosim_gui --postprocess-smoke` 验证跨进程启动及原 Demo 自测；该命令测试完会自动退出。

详细契约、目录结构、接入方法和未完成工作见 [第一阶段集成说明](docs/phase1-integration.md)；实际测试结果见 [构建与运行记录](docs/phase1-validation.md)。Windows 运行目录的打包与启动步骤见 [部署说明](docs/windows-deployment.md)。
