# 开发环境统一说明

本说明以 2026-10-20 对目标 Windows 主机的实际检查为基线。主工程保留 C++14、Qt 5.12.12 和现有 Mock 功能。VTK、SARibbon、JSON 与日志库按模块引入；未接入的库不构成主工程的默认构建依赖。

## 已核验的目标机环境

| 项目 | 实际版本或状态 | 验证依据 |
| --- | --- | --- |
| 操作系统 | Windows 10，10.0.19045.6466，x64 | `ver`、x64 工具链与库配置 |
| IDE / 编译器 | Visual Studio 2017 Community，MSVC v141；`cl` 19.16.27043.0 | `cl /Bv`；CMake 编译器识别 |
| Windows SDK | 10.0.17763.0 | CMake 配置输出 |
| CMake | 3.18.2 | `cmake --version` |
| Git | 2.29.0.windows.1 | `git --version` |
| C++ 标准 | C++14，关闭编译器扩展 | 本仓库顶层 CMake；SARibbon 配置输出亦启用 C++14 |
| 构建架构 | MSVC x64，统一开发配置 **Debug** | `Visual Studio 15 2017 Win64`；VTK 配置文件标明指针宽度 8 字节 |
| Qt | 5.12.12，`msvc2017_64`，Core/Widgets/OpenGL/Svg 可用 | Qt CMake 包；完整技术栈配置与编译 |
| VTK C++ | 8.2.0，64 位，**仅 Debug 导出**，含 `QVTKOpenGLNativeWidget`/`vtkGUISupportQt` | `VTKConfigVersion.cmake`、`VTKTargets-debug.cmake`；Debug 探针编译及运行 |
| SARibbon | 目标机源码标称 2.9.3，当前提交 `0cba0ca` | SARibbon CMake 输出；`RibbonDemo` Release 构建 |
| nlohmann/json | **尚无已验证的安装与版本** | 目前主工程不链接；待 D/G 确定版本与交付方式 |
| 日志库 | **尚未定案或验证** | 设计文档同时提到 log4cpp 与 spdlog；F/A 需确认后再固定版本 |
| Python 后处理 Demo | Python 3.6.6、PySide2 5.12.6、Qt 运行时 5.12.12、VTK Python 9.3.0 | 独立虚拟环境自测；12 组模式及 PNG/VTP 导出 |

Qt C++ 和 VTK C++ 使用 MSVC x64，禁止混用 MinGW、x86 或不同编译器 ABI 的预编译库。Python Demo 在独立进程中运行，Python VTK 9.3.0 **不代表** C++ VTK 8.2.0 的验证。目标机 `vtkGUISupportQt-8.2.dll` 依赖 `Qt5Widgetsd.dll`、`MSVCP140D.dll` 等 Debug DLL；Release 探针虽可链接，但运行时发生堆错误 `0xc0000374`。因此七人统一以 **Debug** 开发与集成；现有 Release C++ 骨架可独立运行，但完整 Release 构建须等匹配的 VTK 8.2.0 Release 包安装后重测。C++ 兼容性以 Debug `aerosim_stack_probe` 为当前证据；C 的正式业务模块仍未交付。

SARibbon 2.9.3（目标机提交 `0cba0ca`）作为本阶段的可验证开发基线；源码获取与后续分发锁定方式还需 B/A 确认。nlohmann/json 的具体版本，以及 log4cpp 与 spdlog 的最终选择尚未确定；这些依赖不得在共享 CMake 中擅自固定或替换。

## 克隆与安装

```bat
git clone https://github.com/Klein1349/AeroSimWorkbench.git
cd AeroSimWorkbench
```

Windows 使用 VS2017 Community 的 **Desktop development with C++** 工作负载、v141 x64 工具集、Windows 10 SDK、CMake 3.18.2 或兼容更新版本，以及 Qt **5.12.12 `msvc2017_64`**。Qt Creator 可作编辑器；生成器仍使用 VS2017 x64。目标机示例位置为 `C:\Qt\5.12.12\msvc2017_64`，这是机器现状，不写入项目构建配置。

C 及集成人员安装或复用 VTK **8.2.0 x64/MSVC2017 Debug**，确认安装树有 `lib/cmake/vtk-8.2/VTKConfig.cmake`、`include/vtk-8.2/QVTKOpenGLNativeWidget.h`、`VTKTargets-debug.cmake` 和对应 Debug DLL。B 及集成人员准备 SARibbon 源码，当前目标机源码为 2.9.3。二者的位置分别通过 `AEROSIM_VTK_DIR` 和 `AEROSIM_SARIBBON_SOURCE_DIR` 指定；不要提交个人路径。SARibbon 的无边框库在 Qt 5.12 下关闭。

现阶段不要求所有成员安装 VTK/SARibbon。D/G 使用 nlohmann/json、F/E 使用日志库之前，先按开发规范确认版本和公共接口，再由模块目标单独声明依赖。求解器、MPI/HPC、CGNS 等工具只由实际使用它们的模块安装。

| 成员 | 基础环境之外需准备或核对的内容 |
| --- | --- |
| A | Qt Core/Widgets、CMake/CTest；集成机还需 VTK 8.2.0 Debug 与 SARibbon 源码以运行全栈探针 |
| B | Qt Widgets/Svg、SARibbon 2.9.3 源码及自己的 UI 目标；VTK 视口仅通过 C 的接口接入 |
| C | VTK 8.2.0 x64/MSVC2017 Debug、Qt OpenGL/Widgets、真实样例网格/结果格式与独立渲染测试 |
| D | Qt Core、待统一版本的 nlohmann/json；本模块参数/校验样例，若需额外数值库单独申报 |
| E | Qt Widgets、公共日志/错误契约；日志后端库由 F/A 定案后按需接入 |
| F | Qt Core、真实求解器可执行文件和输入样例；日志库版本待定，MPI/HPC 本阶段非必需 |
| G | Qt Core、待统一版本的 nlohmann/json、C/D/F 的 Mock 接口与工程保存样例 |

## Windows：统一构建

目标机 CMake 3.18 不支持 `CMakePresets.json`，因此本仓库提供 `scripts\build-windows.cmd`。在 VS2017 开发者命令提示符中运行；环境变量只在本机设置：

```bat
set "AEROSIM_QT_ROOT=C:\Qt\5.12.12\msvc2017_64"
scripts\build-windows.cmd all Debug
build-windows\Debug\aerosim_cli.exe --mock-once
build-windows\Debug\aerosim_gui.exe
```

默认脚本配置、编译全部当前模块并运行 CTest。仅编译公共契约或 Mock 等目标：

```bat
scripts\build-windows.cmd aerosim_contracts Debug
scripts\build-windows.cmd aerosim_application Debug
scripts\build-windows.cmd aerosim_mock Debug
scripts\build-windows.cmd aerosim_ui_mock Debug
scripts\build-windows.cmd aerosim_postprocess_demo_launcher Debug
```

只需无界面服务时，先执行 `set "AEROSIM_BUILD_GUI=OFF"`，再运行脚本；该配置只要求 Qt Core，不查找 Widgets/VTK/SARibbon。恢复 GUI 时将其设为 `ON`，建议另设 `AEROSIM_BUILD_DIR` 以隔离两种缓存。团队成员新增模块时应提供独立 CMake 库目标及自己的测试目标，再由 A 将目标加入主工程；用 `cmake --build <build-dir> --target <module-target>` 即可只编译该模块。当前尚未交付的 B～G 业务目标不能被声称已完成独立构建。

验证完整 Qt/VTK/SARibbon **C++** 工具链时，额外设置本机路径并打开探针：

```bat
set "AEROSIM_BUILD_STACK_PROBE=ON"
set "AEROSIM_VTK_DIR=D:\Project\VTK\lib\cmake\vtk-8.2"
set "AEROSIM_VTK_BIN=D:\Project\VTK\bin"
set "AEROSIM_SARIBBON_SOURCE_DIR=F:\RibbonDemo\RibbonDemo\third_party\SARibbon"
set "AEROSIM_BUILD_DIR=build-stack-probe"
scripts\build-windows.cmd all Debug
```

上述 VTK/SARibbon 路径仅为目标机实例；团队各自设置自己的路径。探针检查 Qt 5.12.12、VTK 8.2.0、64 位，并创建 `SARibbonMainWindow`、`QVTKOpenGLNativeWidget` 和 `vtkGenericOpenGLRenderWindow` 以验证最小 C++ 对象装配；它不显示或渲染正式场景。默认关闭，不影响只安装 Qt 的成员。若只需编译探针：`scripts\build-windows.cmd aerosim_stack_probe Debug`。目标机只有 VTK Debug 导出，探针的 Release 编译会明确失败，避免链接到错误配置。

后处理 Python Demo 可按 [Demo 说明](../postprocess_demo/README.md) 创建 `.venv-qt5`。它不参与 CMake 目标，不作为正式 C++ VTK 模块；主窗口通过独立进程启动它。

## Linux：配置模板（尚未实机验证）

Linux 端必须准备**同版本、同架构**的 Qt 5.12.12 与 VTK 8.2.0 开发库，并从源码编译相容的 SARibbon。编译器 GCC/Clang 的最低实测版本尚未确定；首次 Linux 集成需记录发行版、编译器版本、ABI、VTK Qt 支持及测试结果。不要把 Windows 的 DLL、MSVC 库或 SARibbon 构建目录复制到 Linux。

```sh
git clone https://github.com/Klein1349/AeroSimWorkbench.git
cd AeroSimWorkbench
export AEROSIM_QT_ROOT=/path/to/qt-5.12.12
./scripts/build-linux.sh all
./build-linux/aerosim_cli --mock-once
./build-linux/aerosim_gui
```

如需全栈探针，再设置 `AEROSIM_BUILD_STACK_PROBE=ON`、`AEROSIM_VTK_DIR=/path/to/vtk-8.2/lib/cmake/vtk-8.2`、`AEROSIM_VTK_LIB=/path/to/vtk-8.2/lib`、`AEROSIM_SARIBBON_SOURCE_DIR=/path/to/SARibbon`。`AEROSIM_BUILD_GUI=OFF ./scripts/build-linux.sh all` 可仅构建 CLI。Linux 图形和 OpenGL 运行测试依赖可用显示服务器；无界面环境只保证 CLI 骨架与不要求 GUI 的目标。

## 常见环境错误

| 现象 | 检查与处理 |
| --- | --- |
| 找不到 `Qt5Config.cmake` | 核对 `AEROSIM_QT_ROOT` 指向 Qt 安装树根目录，并确认是 5.12.12 `msvc2017_64` 或 Linux 对应构建。 |
| 找到 Qt 5.15/6 或 x86/MinGW 包 | 删除旧构建目录后重新配置；不要在同一缓存中更换工具链。 |
| 找不到 `VTKConfig.cmake` | 仅全栈探针/C 模块需要；设置 `AEROSIM_VTK_DIR`，确认精确版本 8.2.0 和 Qt 支持模块。 |
| SARibbon 源码路径错误 | `AEROSIM_SARIBBON_SOURCE_DIR` 应含 `src/SARibbonBar/SARibbonBar.h`。 |
| `LNK1112`、无法解析外部符号 | 检查所有库均为 x64/MSVC v141、Debug 对应版本；重新配置独立构建目录。 |
| 全栈 Release 运行异常 `0xc0000374` | 当前 VTK 只导出 Debug；改用统一 Debug 配置。Release 须先取得同版本、同工具链的 VTK Release 安装包并重测。 |
| Windows 运行返回 `0xc0000135` | 将 Qt `bin` 和需要时 VTK `bin` 放入运行时 `PATH`；脚本已为 CTest 添加这些路径。 |
| `cl` 不在 PATH | 使用 VS2017 x64 开发者命令提示符，或调用 `VsDevCmd.bat -arch=amd64`。 |
| `--preset` 无法识别 | CMake 3.18 不支持 Presets；使用本仓库脚本或等价 `cmake -S/-B` 命令。 |
| Qt 平台插件错误 | 避免跨虚拟环境继承 `QT_PLUGIN_PATH`；检查 Qt DLL 与 `platforms/qwindows.dll` 的版本一致。 |

当前验证记录见 [第一阶段验证](phase1-validation.md) 和后续提交的实测记录。Linux 全栈、Windows Release 全栈、JSON/日志库最终版本及其业务模块构建仍需单独验收。

## 可转发的团队通知

> 第一阶段开发环境统一为 C++14、CMake 3.18.2、Qt 5.12.12、VS2017 v141 x64 **Debug**。目标机 VTK 8.2.0 仅提供 Debug 构建，SARibbon 源码为 2.9.3；完整技术栈 Debug 探针和主工程 CTest 3/3 已通过。各成员按 `docs/DevelopmentEnvironment.md` 配置本机路径，用 `scripts/build-windows.cmd <目标> Debug` 独立编译并提交自测结果。不要把 VTK Debug 库链接到 Release；nlohmann/json 具体版本与日志库方案待 A/F/D/G 确认后再统一。Linux 和完整 Release 尚未验收。
