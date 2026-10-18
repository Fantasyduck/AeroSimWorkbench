# 第一阶段构建与运行记录

执行日期：2026-10-18。执行环境：macOS arm64、AppleClang 21.0.0、CMake 4.4.4、Homebrew Qt 5.15.19。

| 验证 | 命令 | 实际结果 |
| --- | --- | --- |
| 完整骨架配置 | `cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt@5 -DCMAKE_BUILD_TYPE=Debug` | 成功生成构建文件 |
| GUI 与 CLI 编译 | `cmake --build build --parallel 6` | `aerosim_gui`、`aerosim_cli` 均编译成功 |
| 自动验收 | `ctest --test-dir build --output-on-failure` | 2/2 通过：CLI 异步闭环、GUI offscreen 启动闭环 |
| CLI 实际运行 | `./build/aerosim_cli --mock-once` | 输出排队命令、Prepared 和 Finished 两个 Mock 事件；退出码 0 |
| GUI 实际启动 | `./build/aerosim_gui` | 进程保持运行，验证后人工终止；`--smoke` 自动退出且测试通过 |
| 无界面配置 | `cmake -S . -B build-cli -DAEROSIM_BUILD_GUI=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt@5 -DCMAKE_BUILD_TYPE=Debug` | 成功，仅查找 Qt Core |
| 无界面编译运行 | `cmake --build build-cli --parallel 6 && ./build-cli/aerosim_cli --mock-once` | 编译成功，异步闭环退出码 0 |

本机没有可用于 macOS 构建的 VTK 8.2 或 SARibbon，也没有 Qt 5.12.12。下载目录可见 Windows 平台的 SARibbon 二进制包，但不能用于本机 arm64 构建。因此本次只能证明 Qt 5.15.19 下的 A 阶段骨架，不能宣称目标版本全栈构建通过。正式接入 B/C 前，应安装同一编译器/架构的 Qt 5.12.12、VTK 8.2 和 SARibbon，并分别提供 `Qt5_DIR`、`VTK_DIR`、`SARibbonBar_DIR` 或 `CMAKE_PREFIX_PATH`，由 B/C 的独立 CMake 目标完成链接验证。

以上 Mock 事件不代表真实参数校验、求解器执行、任务持久化或结果有效性。
