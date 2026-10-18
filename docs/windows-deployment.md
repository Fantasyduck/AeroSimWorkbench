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

要从骨架窗口打开后处理 Demo，需保留 `F:\AeroSimWorkbench\postprocess_demo` 和 `F:\AeroSimWorkbench\.venv-qt5`。后者使用 Python 3.6 x64 和 `postprocess_demo\requirements.txt` 中固定的 PySide2 5.12.6、VTK 9.3.0 与 NumPy 1.19.5。新主机可按 Demo README 创建虚拟环境、安装依赖，然后运行 `postprocess_demo\use_qt_5_12_12.cmd`，把 PySide2 虚拟环境的 Qt DLL 与平台插件换成 `C:\Qt\5.12.12\msvc2017_64` 的版本。点击 **Open post-processing VTK demo** 会在独立进程中启动原窗口；`dist\aerosim_gui.exe --postprocess-smoke` 可验证跨进程调用及 Demo 渲染。目标机 C++ 主程序和 Python Demo 的实际 Qt 运行时均为 5.12.12。

命令行输出中的 `MOCK` 表明只是消息闭环演示，不会运行真实求解器。若在另一台 Windows 主机复制 `dist`，仍需满足相应的 Microsoft Visual C++ 运行库要求。
