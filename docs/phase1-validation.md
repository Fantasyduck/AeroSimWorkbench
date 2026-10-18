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

目标 Windows 10 主机：`F:\AeroSimWorkbench` 更新后，使用 CMake 3.18.2、Visual Studio 2017/MSVC 19.16 和 `C:\Qt\5.12.12\msvc2017_64` 配置成功；Release 模式编译 GUI、CLI 成功。在 `F:\AeroSimWorkbench\build` 运行 `ctest -C Release --output-on-failure`，2/2 测试通过。用 `windeployqt` 将 Qt 运行库打包到 `dist`；在未设置 Qt PATH 的新会话运行 `dist\aerosim_cli.exe --mock-once` 成功。`dist\aerosim_gui.exe` 通过交互式任务在登录用户的桌面会话 1 中持续运行。此记录仅覆盖 Qt 5.12.12 骨架，不覆盖 VTK 8.2 或 SARibbon 的正式模块。

后处理 Demo 接入后重新编译了 Qt 5.12.12 Release 目标，CTest 仍为 2/2 通过。在桌面会话 1 中从打包后的 `dist\aerosim_gui.exe --postprocess-smoke` 发起 Python 子进程，观察到子进程运行，原 Demo 自测完成且整体退出码为 0；随后恢复了常驻 C++ 主窗口。后处理仍为单独的 PySide6/VTK 9 演示进程，不属于正式 C++ VTK 8.2 模块。

2026-10-18：后处理 Demo 已迁移到 PySide2 5.12.6，并在隔离虚拟环境中使用目标机 Qt 5.12.12 SDK 的运行 DLL。目标机运行时打印 `Qt runtime: 5.12.12`；交互式桌面任务自测退出码为 0，12 组变量/显示模式及 PNG、VTP 导出全部通过。Python VTK 为 9.3.0；正式 C++ VTK 8.2 仍待对应模块接入。

以上 Mock 事件不代表真实参数校验、求解器执行、任务持久化或结果有效性。
