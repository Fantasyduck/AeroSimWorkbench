# 正式模块接入区

本目录按七人分工预留 B～G 的正式实现。目前这些目录只有交接说明，**没有可构建的业务目标**；A 阶段的可运行骨架仍使用 `mocks/`。模块交付时由负责人在自己的目录添加 `CMakeLists.txt`、`include/aerosim/<module>/`、`src/` 和必要的 `tests/`，先完成独立目标及自测，再由 A 将目标接入顶层构建和应用装配。

公共数据与端口统一放在 `contracts/`，模块不得复制公共类型。服务模块不依赖 `QWidget`；C 的 VTK 显示控件、结果读取适配和后处理服务可归同一负责人，但须在内部保持 UI、Service、Infrastructure 边界。Qt/VTK/SARibbon 等依赖由实际使用它们的目标声明，不使 CLI 被 GUI 依赖阻断。
