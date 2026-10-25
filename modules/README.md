# 正式模块接入区

本目录预留正式业务模块。当前 `modules/visualization/` 已有一个可独立构建的 VTK 空间特征目标；其余模块仍只有说明，第一阶段可运行骨架仍使用 `mocks/`。新增模块在相应目录添加 `CMakeLists.txt`、`include/aerosim/<module>/`、`src/` 和必要的 `tests/`，先完成独立目标及自测，再接入顶层构建和应用装配。

公共数据与端口统一放在 `contracts/`，模块不得复制公共类型。服务模块不依赖 `QWidget`；VTK 显示控件、结果读取适配和后处理服务可在 `modules/visualization/` 内组织，但须保持 UI、Service、Infrastructure 边界。Qt/VTK/SARibbon 等依赖由实际使用它们的目标声明，不使 CLI 被 GUI 依赖阻断。
