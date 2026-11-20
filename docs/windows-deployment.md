# Windows 第一阶段骨架部署

## 构建目录直接启动

Windows 下构建 `aerosim_gui` 或 `aerosim_postprocess` 后，会自动调用所选 Qt SDK 的 `windeployqt`，按当前 Debug/Release 配置将 Qt DLL、平台及图像插件部署到 EXE 所在目录，同时生成相对路径的 `qt.conf` 和复制可用的 Qt 中文翻译。构建完成后可从该目录双击 EXE。Visual Studio 多配置构建的程序位于对应的 `Debug` 或 `Release` 子目录。

部署步骤会从所选 Qt SDK 补齐对应配置的 Qt OpenGL DLL，例如 Debug 使用 `Qt5OpenGLd.dll`。启用 VTK/CGNS 时，还会使用 MSVC 的 `dumpbin` 递归解析 EXE 的运行库依赖，将所需的第三方 DLL 复制到程序旁边。VTK 目录优先取已导入的 `vtkCommonCore` 目标，额外搜索位置可通过 `AEROSIM_VTK_BIN` 和 `AEROSIM_CGNS_ROOT` 提供。缺失 DLL 会在部署时报告。下载云端构建产物时应保留整个部署目录。

可在 `scripts/build-windows.cmd` 顶部或 `aerosim-windows.local.cmd` 中设置 `AEROSIM_VTK_BIN`，其值为实际 VTK DLL 目录。构建入口会将该目录传入 CMake 并保存，后续 `aerosim_gui_deploy`、`aerosim_postprocess_deploy` 可复用。这里的运行库目录与 `AEROSIM_VTK_DIR` 指定的 VTK CMake 配置目录分别设置。

如果 CMake 已找到 CGNS 导入库，部署也会检查该库所在目录和相邻的 `bin` 目录，包括对应构建配置的子目录。可通过 `AEROSIM_CGNS_ROOT` 补充 CGNS 安装目录或构建目录。下载构建产物后，从完整部署目录启动以加载 CGNS 及其依赖。

若运行库文件被移走，可执行 `cmake --build <构建目录> --config Debug --target aerosim_gui_deploy` 重新部署；独立后处理窗口使用 `aerosim_postprocess_deploy`。部署工具随所选 Qt SDK 提供，构建环境应包含对应的 Visual Studio 编译器环境，以便部署编译器运行库。Debug 构建用于开发机验证，完整 Release 技术栈仍需匹配的 VTK Release 依赖和目标机验收。

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
