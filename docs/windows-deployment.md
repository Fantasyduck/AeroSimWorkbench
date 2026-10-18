# Windows 第一阶段骨架部署

以 Visual Studio 2017 x64、Qt 5.12.12 `msvc2017_64`、CMake 3.18+ 为例。命令在开发者命令提示符中运行；`F:\AeroSimWorkbench` 为源码目录。此过程只部署 A 的 Qt 骨架，C/B 的 VTK 8.2 与 SARibbon 模块尚未接入。

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

要从骨架窗口打开后处理 Demo，需保留 `F:\AeroSimWorkbench\postprocess_demo` 和 `F:\AeroSimWorkbench\.venv`。后者应安装 Python 3.11、`PySide6-Essentials>=6.10,<7`、`vtk>=9.6,<10` 等 `postprocess_demo/requirements.txt` 依赖。目标机此前已部署该环境；新主机可先建虚拟环境并安装 requirements。点击 **Open post-processing VTK demo** 会在独立进程中启动原窗口；`dist\aerosim_gui.exe --postprocess-smoke` 可验证跨进程调用及 Demo 渲染。Qt 5.12 主程序与 Qt 6 Python Demo 不在同一进程中加载。

命令行输出中的 `MOCK` 表明只是消息闭环演示，不会运行真实求解器。若在另一台 Windows 主机复制 `dist`，仍需满足相应的 Microsoft Visual C++ 运行库要求。
