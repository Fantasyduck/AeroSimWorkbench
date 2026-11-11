# Windows 第一阶段骨架部署

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
