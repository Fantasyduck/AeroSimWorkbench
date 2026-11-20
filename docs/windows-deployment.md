# Windows 第一阶段骨架部署

## 从源码构建和启动

在已配置依赖的 VS2017 x64 开发者命令提示符中构建，随后用启动脚本打开主界面。

```bat
scripts\build-windows.cmd all Debug
scripts\run-gui-windows.cmd
```

构建只生成程序和编译产物。启动脚本从本机依赖目录加载 DLL 和 Qt 插件，环境变量只传给本次启动的进程。它从默认的 `build-windows` 或 `AEROSIM_BUILD_DIR` 指定的构建目录定位 EXE，兼容工作台入口使用的 `AEROSIM_WINDOWS_BUILD_DIR`。使用 `scripts\run-gui-windows.cmd -Postprocess` 可打开已编译的独立后处理程序。

依赖路径优先读取环境变量或仓库根目录下的 `aerosim-windows.local.cmd`，缺省时复用构建目录下的 CMake 缓存。Qt 使用 `AEROSIM_QT_ROOT` 或缓存中的 `Qt5_DIR`。启用 VTK 时，可用 `AEROSIM_VTK_BIN` 指定 DLL 目录，或由 VTK 的 CMake 包位置查找标准 `bin` 目录。CGNS 使用 `AEROSIM_CGNS_ROOT` 或已链接导入库附近的目录，也支持直接指定 `AEROSIM_CGNS_BIN`。若 HDF5 DLL 单独安装，可设置 `AEROSIM_HDF5_BIN`。相对路径以仓库根目录为基准。

从其他电脑取得构建产物时，运行机器也需要相同架构和配置的依赖。用全新的构建目录可避免旧构建留下的 DLL 优先加载。完整 Release 技术栈仍需匹配的 VTK Release 依赖和目标机验收。

## 历史 Release 骨架打包流程

以 Visual Studio 2017 x64、Qt 5.12.12 `msvc2017_64`、CMake 3.18+ 为例。命令在开发者命令提示符中运行；`F:\AeroSimWorkbench` 为源码目录。此过程以 **Release 只部署不链接 VTK 的 Qt 骨架**；正式 VTK 8.2/SARibbon 集成当前只在 Debug 配置下验证，不能按下述 Release 命令部署完整技术栈。

```bat
cmake -S F:\AeroSimWorkbench -B F:\AeroSimWorkbench\build -G "Visual Studio 15 2017 Win64" -DCMAKE_PREFIX_PATH=C:\Qt\5.12.12\msvc2017_64
cmake --build F:\AeroSimWorkbench\build --config Release --parallel 6
cd /d F:\AeroSimWorkbench\build
ctest -C Release --output-on-failure

mkdir F:\AeroSimWorkbench\dist
copy /Y F:\AeroSimWorkbench\build\Release\aerosim_cli.exe F:\AeroSimWorkbench\dist\
copy /Y F:\AeroSimWorkbench\build\Release\aerosim_gui.exe F:\AeroSimWorkbench\dist\
C:\Qt\5.12.12\msvc2017_64\bin\windeployqt.exe --release --no-translations F:\AeroSimWorkbench\dist\aerosim_gui.exe
F:\AeroSimWorkbench\dist\aerosim_cli.exe --mock-once
```

`dist\aerosim_gui.exe` 可在已登录的 Windows 桌面直接启动。当前目标机还注册了手动触发的交互式计划任务 `AeroSimWorkbench-CPP-GUI`，可运行 `schtasks /run /tn AeroSimWorkbench-CPP-GUI` 再次打开窗口。该任务不会在开机时自动运行。打包目录 `dist` 不纳入 Git；修改源码后需重新构建、复制 EXE，并再次运行 `windeployqt`。

本 Release 部署包包含 Qt 后处理工作台。构建 GUI 时可视化模块默认开启（`AEROSIM_BUILD_VISUALIZATION` 随 `AEROSIM_BUILD_GUI`），因此这类构建除 Qt 外还需要 Qwt 7.5 与 Eigen 3；只想发布不带监控曲线的界面可用 `-DAEROSIM_BUILD_VISUALIZATION=OFF` 配置。未启用 VTK 时中央区域仍是明确标记的视口占位画布；启用实际 `VtkSceneViewer` 需改用 Qt 5.12.12 / VTK 8.2.0 的匹配 **Debug** 构建并设 `AEROSIM_VTK_DIR` 与 `AEROSIM_ENABLE_VTK_VIEWER=1`；结果读取、场景对象管理及正式后处理服务仍需单独验证。

命令行输出中的 `MOCK` 表明只是消息闭环演示，不会运行真实求解器。若在另一台 Windows 主机复制 `dist`，仍需满足相应的 Microsoft Visual C++ 运行库要求。
