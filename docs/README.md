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
| [第一阶段主工程集成说明](phase1-integration.md) | 当前 GUI/CLI、ApplicationController、contracts、Mock 和正式模块接入边界 | 现行骨架说明；正式业务目标尚未接入 |
| [公共数据契约升级设计与实施计划](公共数据契约升级设计与实施计划.md) | 工作流、State/Batch/Task、多网格、资源与查询端口的增量升级规范及分 PR 计划 | PR 0 设计冻结；PR 1/2 已实现；PR 3～PR 7 数据模型已加入 `contracts/`，业务服务与查询端口未实现 |

## 后处理接口和功能规范

| 文档 | 用途与适用范围 | 状态 |
| --- | --- | --- |
| [后处理功能与界面交互说明](PostProcessUiFunctionGuide.md) | 后处理操作流程、界面布局、基础视图操作及功能交互 | 功能与交互目标；部分界面仍为草稿 |
| [后处理集成准备](PostProcessIntegrationGuide.md) | 主窗口/工作区/VTKViewer 的关系、数据与场景所有权、分批接入路径 | 集成方案待评审 |
| [后处理公共契约](PostProcessContracts.md) | 区分已有引用/端口、已加入的轻量标识及待确认的数据/场景协议 | 部分已实现；其余为接口草案 |
| [后处理功能交接表](PostProcessTaskHandoff.md) | 保留 16 项功能编号、输入输出、依赖、建议接入位置及测试条件 | 功能要求保留；目标/API 待评审 |

## 测试数据与验证

| 文档 | 用途与适用范围 | 状态 |
| --- | --- | --- |
| [后处理测试数据](PostProcessTestData.md) | 小型解析夹具、CMake 完整性检查及数值预期 | 数据已提交并有无 Python 的 CTest 检查；正式 C++ VTK 8.2 读取尚未验证 |

## 构建部署及历史记录

| 文档 | 用途与适用范围 | 状态 |
| --- | --- | --- |
| [第一阶段验证记录](phase1-validation.md) | 按日期保存曾执行的构建、CTest 和技术栈探针结果 | 历史记录；已退役原型的结果不代表当前功能 |
| [Windows 骨架部署](windows-deployment.md) | 不链接 VTK 的 Qt GUI/CLI Release 骨架打包与启动 | 骨架范围有效；不适用于正式 VTK Release 集成 |

当前根 CMake 已接入 `modules/ui` 主窗口、Mock 服务、夹具检查和可选技术栈探针。参见 [UI 模块说明](../modules/ui/README.md) 和 [UI 验证记录](ui-validation.md)。其余正式业务目标尚未定义。后处理建议目标名与场景 API 应在接口评审后再写入构建配置。
