# Windows 主窗口中文修复验证（2026-10-29）

基线：`533e7ff`。目标环境：Windows 10、VS2017 v141 x64 Debug、Qt 5.12.12、现有 SARibbon 2.9.3；构建目录为目标机的 `build-windows`。

## 问题与修复

主窗口实际标题含错误 Unicode 字符（例如原本的中点 U+00B7 已成为 U+8DEF），并非只有 SSH 终端显示错误。检查生成的 VS 工程发现已有 `/utf-8`，因此未用重复增加编译开关作为修复。新增 ASCII 源码的 UTF-16 期望值测试在旧 UI 库上返回 12：`Window title has incorrect Unicode characters`。

主窗口沿用既有后处理 UI 的 `QString::fromUtf16` 写法：中文常量使用显式 `u"\u...."` 字面量，包括 Ribbon 分组、操作、树节点、属性及提示文字。分组存储改为 QString，去掉窄字符串中转；保留既有编译版本和业务行为。回归测试检查标题、Dock、工程树、Ribbon 操作、属性标题的实际 Unicode 值。后处理测试中“平移”的期望值也改为显式 UTF-16，避免测试本身依赖源字符集。

此修复消除源/执行字符集对这些文案的依赖；未据此认定 Qt 库有缺陷，也未修改目标系统语言或字体设置。

## 实测结果

- 修复前：新增主窗口中文断言失败，退出码 12。
- 修复后：Windows Debug 全量编译通过；主窗口中文及所有权/服务绑定测试通过。
- 后处理 UI smoke 的中文断言修正后通过；`ctest -C Debug -R "ui_smoke|ui_window" --output-on-failure` 为 2/2。
- 原生 Windows 平台：`aerosim_gui --smoke -platform windows`、`aerosim_gui --postprocess --smoke -platform windows` 均等待进程结束并确认退出码 0。
- 在交互式桌面用 `--screenshot` 导出真实窗口图，确认标题、九类 Ribbon 页签、操作、树节点、属性区、日志和状态栏中文正确；截图退出码 0。重新打开默认主窗口和后处理窗口。
- 全量 CTest 初轮为 3/6；其中后处理测试的中文期望值问题已修正。两个 `QT_QPA_PLATFORM=offscreen` GUI smoke 仍发生异常退出，增加本机字体目录后仍复现，未宣称全量 CTest 通过。相同两个程序改用 windows 平台均通过；offscreen 差异需单独排查。

本次仅验证 UI 与 Mock 通信，不涉及正式 VTK 渲染、求解器或业务数据。没有升级依赖、改动系统编码或删除用户截图。
