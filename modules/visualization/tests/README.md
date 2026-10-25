# 手动运行空间特征测试（Windows）

在 **VS2017 x64 开发者命令提示符**中进入仓库根目录，设置本机路径：

```bat
set "AEROSIM_QT_ROOT=C:\Qt\5.12.12\msvc2017_64"
set "AEROSIM_VTK_DIR=D:\Project\VTK\lib\cmake\vtk-8.2"
set "AEROSIM_VTK_BIN=D:\Project\VTK\bin"
scripts\test-spatial-features.cmd
```

VTK 路径按实际安装修改。脚本独立配置、编译并运行 `aerosim_spatial_features_test.exe`，使用 C++14、VTK 8.2.0、Qt Core 5.12.12、x64 Debug，不需要界面、SARibbon、Python 或求解器。

预期末尾为：

```text
RESULT: 9 passed, 0 failed
```

任何失败返回非零退出码，具体组显示 `[FAIL]` 和原因。程序开头打印实际 Qt/VTK 版本。测试输出文件创建在临时目录，结束后自动清理，不修改公共夹具。

编译完成后单独重复执行：

```bat
build-spatial-features\Debug\aerosim_spatial_features_test.exe tests\fixtures\postprocess
```

保持 Qt/VTK `bin` 在 PATH。也可进入 `build-spatial-features` 后执行 `ctest -C Debug --output-on-failure`。

测试包含：多个独立/斜切面；切片圆形等值线和选定外表面温度等值线；球形等值面；仓库三帧四字段；四类几何 VTP 回读；字段选择、中文路径与安全写出；非法参数、空结果、退化等值；显式单元插值；后台快照、取消和结果生命周期。

这是算法与文件接口测试，不验证正式 Viewer、主窗口、真实求解器格式或完整前后处理工作流。
