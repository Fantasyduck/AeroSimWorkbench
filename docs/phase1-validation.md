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

## 2026-10-20 开发环境统一验证

目标机为 Windows 10 `10.0.19045.6466`，VS2017 Community/MSVC `19.16.27043.0`、x64、Windows SDK `10.0.17763.0`，CMake `3.18.2`。Qt 为 `5.12.12 msvc2017_64`。`D:\Project\VTK\lib\cmake\vtk-8.2\VTKConfigVersion.cmake` 报告 `8.2.0` 且指针宽度 8 字节；SARibbon 源码 CMake 报告 `2.9.3`，Git 提交 `0cba0ca`。

| 验证 | 实际结果 |
| --- | --- |
| 现有 `RibbonDemo` Qt/VTK/SARibbon Release 目标 | **编译和链接通过**；未据此宣称 Release 运行安全 |
| 主仓库完整技术栈探针，MSVC x64 Debug | 创建 `SARibbonMainWindow`、`QVTKOpenGLNativeWidget` 和 `vtkGenericOpenGLRenderWindow`，编译链接及最小对象装配运行通过；未显示/渲染场景 |
| 主仓库 Debug 全量构建与 CTest | `aerosim_gui`、`aerosim_cli`、探针均成功；CTest **3/3** 通过 |
| 关闭 GUI 的 Debug 独立构建 | 仅要求 Qt Core，`aerosim_cli` 构建成功；CTest **1/1** 通过；单独编译 `aerosim_contracts` 目标成功 |
| 原 VTK 包用于 Release 探针运行 | **失败**，退出码 `0xc0000374`；调查看到 VTK 只导出 `VTKTargets-debug.cmake`，`vtkGUISupportQt-8.2.dll` 依赖 `Qt5Widgetsd.dll` 和 `MSVCP140D.dll`。新增保护后，Release 探针按预期以清晰的 `#error` 拒绝编译 |

目标机现有 C++ VTK 8.2.0 **只能作为 Debug 开发依赖**。完整 Release 集成尚需同编译器、x64 的 VTK 8.2.0 Release 库并重新验证。探针不包含正式 C 模块或渲染正确性测试；B～G 业务模块尚未交付。检查主仓库和已知第三方目录未找到可确认的 nlohmann/json 与 log4cpp 安装，具体版本/日志库方案仍待确定。Linux 与本机 macOS Qt 5.12.12 全栈本轮未验证。

以上 Mock 事件不代表真实参数校验、求解器执行、任务持久化或结果有效性。

## 2026-10-22 目录统一后的验证

在提交 `6644d6d` 中，仅迁移 A 的入口与装配、公共 UI 契约、Mock 和独立后处理 Demo，保留既有可执行目标名及构建输出位置。`modules/` 目前是 B～G 的交接目录，尚无正式业务目标。

| 验证环境与项目 | 实际结果 |
| --- | --- |
| 本机 macOS arm64 临时源码副本；只在副本中将 CMake 的 Qt 精确版本改为已安装的 5.15.19，以检查目录迁移 | GUI/CLI 编译成功，CTest **2/2**；GUI 关闭时 CLI 编译成功，CTest **1/1**。此项不代表 Qt 5.12.12 本机验证 |
| 目标 Windows 10、VS2017 v141 x64、Qt 5.12.12；独立 Git worktree，VTK 8.2.0/SARibbon 2.9.3 探针开启，Debug | `scripts\build-windows.cmd all Debug` 配置及编译成功，CTest **3/3**（CLI、GUI、技术栈探针） |
| 同一 Windows worktree，关闭 GUI 与探针，独立 Debug 构建目录 | 只使用 Qt Core，CLI 构建成功，CTest **1/1** |
| 同一 Windows worktree，使用现有 `.venv-qt5` 目录联接，在登录用户的交互式桌面运行新 GUI 的 `--postprocess-smoke` | 退出码 **0**；验证 Demo 从 `demos/postprocess/` 定位并完成子进程自测 |

上述首轮 Windows 验证使用临时 worktree `F:\AeroSimWorkbenchRuntime\layout-check`。随后将正式仓库 `F:\AeroSimWorkbench` 更新到 `066e776`：同配置 Debug 完整构建及 CTest **3/3** 通过；关闭 VTK/SARibbon 探针后的 Qt 骨架 Release 构建及 CTest **2/2** 通过。新 Release GUI/CLI 已复制到 `dist/` 并重新运行 `windeployqt`；打包 CLI 的 `--mock-once` 退出码为 0，登录用户交互式桌面中从 `dist/` 运行 GUI `--postprocess-smoke` 的 `EXIT_CODE=0`。Release 验证只覆盖不链接 VTK 的骨架，**不代表 VTK 8.2 Release 全栈可用**。B～G 的业务模块仍未交付，本记录不涵盖真实工作流、求解、VTK 数据分析正确性或 Linux 构建。
