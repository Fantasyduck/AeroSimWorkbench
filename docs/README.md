# 技术文档导航

按当前任务选择相应文档；根目录 [README](../README.md) 给出快速开始，[AGENTS.md](../AGENTS.md) 给出仓库开发约定。`modules/*/README.md` 描述各正式模块的技术职责。下表“现行”指当前仓库的规则或结构，“待评审”不代表接口已经冻结，“历史记录”保留当时实测而不替代当前配置。

## 开发环境与工程规范

| 文档 | 用途与适用范围 | 状态 |
| --- | --- | --- |
| [开发环境说明](DevelopmentEnvironment.md) | C++14、Qt 5.12.12、VS2017 x64 Debug 基线，依赖配置及 Windows/Linux 构建命令 | 已验证 Windows 基线；Linux 和完整 Release 未验证 |
| [工程开发规范](DevelopmentGuide.md) | 目录、模块依赖、C++/CMake、公共契约、PR 与独立测试约定 | 现行规范 |

## 软件架构与模块集成

| 文档 | 用途与适用范围 | 状态 |
| --- | --- | --- |
| [第一阶段主工程集成说明](phase1-integration.md) | 当前 GUI/CLI、ApplicationController、contracts、Mock 和正式模块接入边界 | 现行骨架说明；空间特征目标已可选接入 |

## 后处理接口和功能规范

| 文档 | 用途与适用范围 | 状态 |
| --- | --- | --- |
| [后处理集成准备](PostProcessIntegrationGuide.md) | 主窗口/工作区/VTKViewer 的关系、数据与场景所有权、分批接入路径 | 空间特征算法已实现；其余集成方案待评审 |
| [后处理公共契约](PostProcessContracts.md) | 区分已有引用/端口、已加入的轻量标识及待确认的数据/场景协议 | 部分已实现；其余为接口草案 |
| [后处理功能交接表](PostProcessTaskHandoff.md) | 保留 16 项功能编号、输入输出、依赖、建议接入位置及测试条件 | ④⑤⑥⑭已有模块内算法；其余目标/API 待评审 |

## 测试数据与验证

| 文档 | 用途与适用范围 | 状态 |
| --- | --- | --- |
| [后处理测试数据](PostProcessTestData.md) | 小型解析夹具、CMake 完整性检查及数值预期 | 数据已提交并有无 Python 的 CTest 检查；正式 C++ VTK 8.2 读取尚未验证 |

## 构建部署及历史记录

| 文档 | 用途与适用范围 | 状态 |
| --- | --- | --- |
| [第一阶段验证记录](phase1-validation.md) | 按日期保存曾执行的构建、CTest 和技术栈探针结果 | 历史记录；已退役原型的结果不代表当前功能 |
| [Windows 骨架部署](windows-deployment.md) | 不链接 VTK 的 Qt GUI/CLI Release 骨架打包与启动 | 骨架范围有效；不适用于正式 VTK Release 集成 |

当前根 CMake 默认只接入骨架、Mock、夹具检查和可选技术栈探针。开启 `AEROSIM_BUILD_VISUALIZATION=ON` 后接入可视化模块；目前其中只有空间特征算法目标。后处理其余目标和场景 API 仍须接口评审后再实现。
