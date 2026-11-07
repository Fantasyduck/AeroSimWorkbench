# Implementation Plan: Workflow V2 工作流状态模型升级

**Branch**: `chore/adopt-sdd`（Spec Kit 功能键：`001-workflow-v2-state`） | **Date**: 2026-11-07 | **Spec**: [spec.md](./spec.md)

**Input**: 已完成 Specify 和 Clarify 的 `specs/001-workflow-v2-state/spec.md`，以及当前公共契约、实现代码、项目宪法和已冻结的《公共数据契约升级设计与实施计划》。

**Scope rule**: 本计划只描述 PR 2 的技术设计。它不修改业务 C++、不生成 `tasks.md`、不进入 Implement，也不提前实施 PR 3 的 UI 迁移或 JSON 持久化。

## Summary

Workflow V2 将完成事实、校验事实和依赖阻塞拆分建模：已提交配置事实保留 `Completed`，当前输入与最近一次有效校验的精确输入印章不匹配时派生为 `Stale`，依赖阻塞则由工作流定义中显式声明的条件动态推导。编辑草稿是运行期叠加层，不会因输入框变化而改写已提交事实；异步校验仅在 request generation 与完整输入印章均匹配当前请求时提交。

公共契约以只读 `WorkflowViewV2` 增量扩展 `IWorkflowService`，旧 `WorkflowView` 由 V2 状态无状态映射生成，以保持现有 UI 和调用方可编译、可运行。生产状态机落在新的 `modules/workflow/` Qt Core 模块，Mock 复用相同推导语义；动作授权由服务在请求时重新检查完成、校验、依赖和版本条件。测试分为公共契约、状态推导/异步协调、Mock/兼容集成三层，并接入独立模块构建和顶层 CTest。

## Technical Context

**Language/Version**: C++14

**Primary Dependencies**: Qt 5.12.12 Core；现有 `aerosim_contracts` 公共契约；不引入 QWidget、VTK、SARibbon 或新的第三方库

**Storage**: PR 2 采用存储无关的内存 facts snapshot 与 restore 接口；不选择 JSON 库、不定义 JSON 文件格式

**Testing**: CMake/CTest 驱动的轻量 C++ 测试可执行文件，使用 `QCoreApplication`、lambda 和事件投递验证 Qt 信号与异步行为

**Target Platform**: 已验证基线为 Windows x64、VS2017 v141、Debug；Linux/macOS 仅可作为辅助 headless 检查，不形成新的支持声明

**Project Type**: Qt 桌面应用中的公共契约库、Qt Core 服务模块和 Mock 适配层；公共服务保持无 GUI 可用

**Performance Goals**: 单次视图推导和动作授权在步骤数、依赖数、动作数和相关输入数上保持线性；不进行文件 I/O、资源内容扫描或跨 revision 缓存搜索

**Constraints**: 严格保持 UI → Service → Infrastructure/Adapter；服务线程唯一提交 facts；跨线程消息按值传递并使用 `Qt::QueuedConnection`；旧接口保持源兼容和行为兼容；精确版本规则不得被相同 fingerprint 绕过

**Scale/Scope**: 单个 scope 的工作流定义、步骤事实、输入印章、多个原子 blocker 和动作授权；覆盖现有 Mock、CLI/无界面调用和旧 UI 消费路径，不包含 PR 3 的 UI 改造

## Constitution Check

*GATE: Phase 0 研究前检查；Phase 1 设计完成后再次检查。*

| 原则 | Phase 0 | Phase 1 复核 | 设计证据 |
| --- | --- | --- | --- |
| I. 分层架构 | PASS | PASS | 状态推导、校验协调和动作授权位于 `modules/workflow/`；UI 只消费授权结果；`ApplicationController` 不承载规则。 |
| II. 公共契约评审 | PASS | PASS | V2 类型集中在 `contracts/`；公共层只暴露轻量只读视图；调用方、兼容映射、线程和生命周期均在契约设计中明确。 |
| III. 线程、UI 和生命周期 | PASS | PASS | 服务所属线程唯一提交 facts；worker 结果按值排队；新信号参数注册元类型；待处理请求具备 generation、stamp 和销毁时取消规则。 |
| IV. 模块独立构建与测试 | PASS | PASS | 规划独立 `aerosim_workflow` 目标和三层测试；headless 构建不依赖 GUI、VTK 或 SARibbon；Mock 测试不声称真实求解正确性。 |
| V. 可复现工具链 | PASS | PASS | 保持 C++14、Qt 5.12.12、VS2017 v141 x64 Debug、CMake 3.18.2；不增加依赖，不决定 nlohmann/json。 |

没有宪法例外或需跟踪的复杂性豁免。

## Project Structure

### Documentation (this feature)

```text
specs/001-workflow-v2-state/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── workflow-v2-contract.md
├── checklists/
│   └── requirements.md
└── spec.md
```

`tasks.md` 属于后续 `$speckit-tasks` 阶段，本阶段不创建。

### Source Code (repository root; implementation targets only)

```text
contracts/
├── include/aerosim/contracts/
│   ├── WorkflowModels.h          # V2 公共只读模型和稳定 token 声明
│   └── TaskPorts.h               # viewV2() 与 V2 变更信号
├── src/
│   ├── TaskPorts.cpp             # viewV2() 默认 Unsupported 实现
│   └── RegisterMetaTypes.cpp     # WorkflowViewV2 元类型注册
└── tests/
    └── WorkflowV2ContractTests.cpp

modules/workflow/
├── CMakeLists.txt
├── include/aerosim/workflow/
│   ├── WorkflowDefinition.h
│   ├── WorkflowFacts.h
│   ├── WorkflowSnapshot.h
│   └── WorkflowService.h
├── src/
│   ├── WorkflowStateDeriver.cpp
│   ├── ValidationStampMatcher.cpp
│   ├── DependencyEvaluator.cpp
│   ├── ActionAuthorizer.cpp
│   ├── LegacyWorkflowAdapter.cpp
│   ├── ValidationCoordinator.cpp
│   └── WorkflowService.cpp
└── tests/
    ├── WorkflowStateTests.cpp
    ├── WorkflowAsyncValidationTests.cpp
    └── WorkflowSnapshotTests.cpp

mocks/
├── services/MockWorkflowService.h
├── services/MockWorkflowService.cpp
└── tests/MockWorkflowV2Tests.cpp

CMakeLists.txt                       # 接入 workflow 子目录和顶层 CTest
```

**Structure Decision**: 复用现有 `contracts/` 和 `mocks/`，新增正式 `modules/workflow/` Qt Core 目标 `aerosim_workflow`（并提供项目约定的 alias）。公共模型只表达跨模块读视图；工作流定义、权威 facts、输入印章、快照和协调器均为模块内部类型，避免冻结未必要的写契约。

## Design Overview

### 1. 公共 V2 数据模型

- `CompletionState` 独立表达 `NotStarted`、`InProgress`、`Completed`。
- `ValidationState` 独立表达 `NotValidated`、`Valid`、`Invalid`、`Stale`。
- `DependencyBlockerView` 是原子 blocker，至少携带依赖来源、未满足条件的稳定 code、可展示原因和修复目标。
- `WorkflowStepViewV2` 同时携带 completion、validation、零到多个 blockers、scope、适用性、导航信息和 `factRevision`。
- `WorkflowViewV2` 携带 `definitionVersion`、`viewRevision`、步骤、动作、问题和下一步建议。
- 枚举对外稳定 token 为 `not_started`/`in_progress`/`completed` 与 `not_validated`/`valid`/`invalid`/`stale`；未知 token 必须显式报错，不能静默映射。

公共契约不暴露可写 facts、pending request、草稿、依赖定义或存储格式。具体字段和不变量见 [data-model.md](./data-model.md) 与 [workflow-v2-contract.md](./contracts/workflow-v2-contract.md)。

### 2. 权威事实与状态推导

`WorkflowFactsStore` 按 scope 保存最近一次已提交的步骤事实和历史校验事实。`WorkflowDefinition` 显式声明步骤适用性、依赖条件和动作规则。`WorkflowStateDeriver` 使用 definition、facts、当前输入和可选编辑叠加层纯函数式地产生 V2 视图：

1. 从已提交事实确定基础 completion；当前步骤存在编辑会话时，视图叠加为 `InProgress`。
2. 以唯一 `ValidationStampMatcher` 比较最近校验印章与当前相关输入，推导 validation。
3. `DependencyEvaluator` 只逐条计算当前步骤显式声明且适用的依赖，生成独立 blockers。
4. `ActionAuthorizer` 综合 completion、validation、blockers 和动作要求的输入版本，在请求到达时重新授权。

取消编辑或保存失败仅移除/保留草稿，不改变已提交 facts、facts revision 或校验记录。成功保存提交新事实：原完成事实恢复为 `Completed`，但 validation 仍由新印章比较决定，绝不自动恢复 `Valid`。

### 3. 精确输入印章

`ValidationInputStamp` 固定记录实际校验使用的 scope、`definitionVersion`、相关参数 identity/revision、相关资源 identity/revision，以及参与校验并已验证的 fingerprint。资源集合在比较前以稳定 identity 排序并拒绝重复 identity。

- 任一相关项不相等即为 `Stale`。
- resource revision 不同，即使 fingerprint 相同也不复用校验。
- fingerprint 缺失或未验证不能证明内容相同，也不能绕过 revision 检查。
- fingerprint 比较覆盖算法、摘要和大小等已验证字段。
- location、mtime 与纯 `MeshPresentation` 属性不进入计算输入印章。
- PR 2 不做跨 revision 校验缓存复用。

### 4. 异步校验协调

`ValidationCoordinator` 为每个 scope/step 维护单调递增 request generation，并将 generation 与请求时的完整 stamp 一起传给 worker。结果回到服务所属线程后，只有同时满足“仍是当前 generation”和“结果 stamp 精确匹配当前输入”才提交校验事实；否则静默丢弃，并且不递增 `factRevision` 或 `viewRevision`。

新请求替代旧请求、scope 销毁或服务关闭时，旧 generation 失效。worker 不直接访问或修改 facts；跨线程返回使用轻量值对象与 `Qt::QueuedConnection`。服务对象负责连接和 pending 请求生命周期，禁止重复连接。

### 5. 依赖阻塞与恢复

每项 `DependencyRequirement` 明确上游步骤、条件种类（上游完成、当前校验有效、输入版本匹配）和稳定 reason code。多个依赖分别计算：不满足时只新增对应 blocker，恢复时只删除对应 blocker，不修改下游完成事实或历史校验事实，也不清除其他 blocker。

上游重新满足依赖后，下游 blocker 可以解除；若上游资源版本已经导致下游校验印章失配，下游仍保持 `Stale`，直到针对当前输入重新校验成功。未声明或不适用的依赖不参与计算。

### 6. 动作门控

`Completed + Stale` 只阻止新计算执行和其他明确要求当前有效校验的动作。浏览、导航、编辑、保存和重新校验始终可用；批处理计划可以创建、编辑和保存，但提交新批处理执行时必须重新授权。已创建任务和历史结果继续依据其不可变执行快照及任务状态机，不被当前工作流状态追溯取消。

UI 展示 `ActionAuthorizer` 的结果，不复制或绕过规则。`requestRun()` 等真正产生执行的入口必须再次检查 completion、validation、blockers 和当前 stamp，防止视图与提交之间的竞态。

### 7. 快照和恢复

`WorkflowFactsSnapshot` 是存储无关的值对象，保存 definition 标识/版本、scope、已提交步骤事实、校验事实和必要 revision。restore 先完整校验 snapshot，再原子替换 facts，最后用当前 definition、输入和依赖重新推导视图。

草稿、pending validation、派生 blockers、派生 validation、动作授权和旧视图不序列化。PR 2 只验证 snapshot round-trip 与派生重算，不设计 JSON schema、不选择 JSON 库、不实现工程文件读写。

## `IWorkflowService` and Legacy Compatibility

`IWorkflowService` 增加非纯虚的同步查询：

```cpp
virtual AccessResult<WorkflowViewV2> viewV2(const ScopeRef& scope) const;
```

基类 out-of-line 默认实现返回稳定错误 `workflow.v2.unsupported`，因此现有第三方/Mock 派生类无需立即实现新纯虚函数。服务新增按值信号：

```cpp
void workflowViewV2Changed(aerosim::WorkflowViewV2 view);
```

`WorkflowViewV2` 使用 `Q_DECLARE_METATYPE` 并在公共注册入口中执行 `qRegisterMetaType`；`AccessResult` 仅作同步返回，不注册为信号类型。

正式 V2 服务以 facts 为唯一权威源，同时通过 `LegacyWorkflowAdapter` 发出旧 `WorkflowView`。旧状态映射冻结为：存在 blocker → `Blocked`；否则 `Stale`/`Invalid` → `NeedsValidation`；否则 `Completed + Valid` → `Complete`；其余 → `Pending`。旧 `actionId == "run"` 和现有 UI 消费路径保持可用，但执行入口仍以 V2 授权结果为准。旧信号和 V2 信号从同一已提交 revision 生成，避免观察到互相矛盾的状态。

## Module Change Plan

| 模块 | 计划修改 | 依赖/边界 |
| --- | --- | --- |
| `contracts/` | 增加 V2 只读模型、稳定 token、`viewV2()` 默认实现、新信号与元类型注册 | 仅依赖 Qt Core；保持旧签名和源码调用方式，不暴露实现 facts；仓库统一重编译，不承诺 QObject/vtable 二进制 ABI 不变 |
| `modules/workflow/` | 新增定义、facts store、纯推导器、印章匹配、依赖计算、动作授权、异步协调、snapshot 和 legacy adapter | 不依赖 QWidget、VTK、SARibbon；由服务线程拥有可变状态 |
| `mocks/` | 扩展 Mock 服务以产生可控 V2 facts、模拟乱序校验与同时发布新旧视图 | 复用正式状态规则或共享无 UI 核心，不复制另一套业务语义 |
| `modules/ui/` | PR 2 不改业务 UI；仅由现有旧视图路径验证兼容性 | V2 UI 迁移属于 PR 3 |
| `app/` | 仅在后续实现确有需要时装配服务和连接，不放入规则 | 本计划不修改代码；禁止把推导逻辑移入 controller |
| 根 CMake | 增加独立 workflow 子目录、测试目标和 headless CTest 接入 | GUI 关闭时仍可构建和测试 contracts/workflow/mocks |

## Compatibility Risks and Mitigations

| 风险 | 影响 | 缓解与验证 |
| --- | --- | --- |
| 给 `IWorkflowService` 增加接口破坏现有派生类 | 旧 Mock 或外部实现无法编译 | 使用非纯虚、out-of-line 默认 Unsupported；增加未覆盖 `viewV2()` 的 legacy derived class 编译测试 |
| 新旧视图映射丢失 V2 细节 | 旧 UI 无法展示多 blocker 或 Stale 细因 | 明确为有损兼容层；旧 UI 保持原行为，V2 消费者读取新视图；两视图同 revision 发布 |
| 信号跨线程传输失败 | 运行期警告或丢消息 | 值传递、`Q_DECLARE_METATYPE`、集中注册、真实 queued signal 测试 |
| 乱序校验覆盖新输入 | 错误恢复 `Valid` 并允许执行 | generation + exact stamp 双门控；测试 A/B 乱序、取消、销毁和重复回调 |
| blocker 恢复误改下游事实 | 丢失完成事实或历史校验 | blocker 始终派生；逐 blocker 恢复测试并断言 facts 未变 |
| fingerprint 被错误用于跨 revision 复用 | 旧校验被当作当前有效 | matcher 固定比较 resource revision；相同 fingerprint/不同 revision 必须 Stale |
| UI 依据缓存视图直接执行 | 视图到提交间输入变化造成绕过 | 所有产生新执行的服务入口再次授权；UI 仅展示授权结果 |
| snapshot 被误解为 JSON 持久化承诺 | 提前冻结格式或引入未决依赖 | 只定义存储无关值对象和 round-trip；JSON/工程保存恢复明确排除 |
| `factRevision` 与草稿/派生变化混淆 | 异步结果和视图去重不可靠 | facts 变更才递增 `factRevision`；所有可观察视图变化由 `viewRevision` 表达；丢弃结果不递增任何 revision |

## Test Plan

### Contract tests

- V2 枚举稳定 token 正反向转换和未知 token 拒绝。
- `WorkflowViewV2` 元类型注册及 queued signal 按值传输。
- 未实现 V2 的旧派生类仍可实例化，`viewV2()` 返回 `workflow.v2.unsupported`。
- V2 模型保持轻量、可复制，不包含 QWidget/VTK 或模块内部 facts。

### Workflow unit tests

- `Completed + Valid`、编辑叠加 `InProgress`、取消/失败不改 facts、保存后 `Completed + Stale`。
- definitionVersion、参数 identity/revision、资源 identity/revision、已验证 fingerprint 各字段变化的精确印章矩阵。
- 相同 fingerprint 但 revision 不同仍 Stale；缺失/未验证 fingerprint 不绕过；纯 MeshPresentation 变化不失效。
- 多 blocker 的独立产生、独立解除、无关步骤不阻塞，以及上游恢复后下游仍因印章失配保持 Stale。
- 动作矩阵：浏览/编辑/保存/重新校验允许；新计算和批执行提交拒绝；批计划编辑保存允许；历史任务不受当前 Stale 追溯影响。
- snapshot round-trip、无草稿/pending/blocker 持久化、restore 后按当前 definition/input 重算。
- legacy 状态映射和同 revision 新旧视图一致性。

### Async and integration tests

- 请求 A 后输入变化并发起 B，按 B→A 与 A→B 顺序返回，只有当前 generation + stamp 的结果可提交。
- 旧结果、重复结果、取消结果、scope 销毁后的结果均不改变 facts/view revision，也不发出误导性状态更新。
- 跨线程 worker 结果通过 queued connection 回到服务线程，断言所有 facts mutation 发生在拥有线程。
- Mock 可控制完成、校验、依赖、印章和返回顺序；现有旧 UI/装配测试继续通过，新 V2 信号可被无界面测试消费。
- `requestRun()` 在状态发布后输入变化的竞态下重新授权并拒绝执行。

### CMake and validation commands

实现阶段应先运行独立模块测试，再运行顶层 headless 构建和 CTest。Windows 基线命令以 [quickstart.md](./quickstart.md) 为准，包括：

```bat
scripts\build-windows.cmd aerosim_workflow_tests Debug
scripts\build-windows.cmd all Debug
ctest --test-dir build --build-config Debug --output-on-failure
```

如脚本目标粒度与实际 CMake 入口不同，Implement 阶段应在不改变支持基线的前提下记录精确可复现命令。Linux/macOS 结果只能报告为辅助 headless 检查，不能替代 Windows v141 x64 Debug 验证。

## Delivery Boundaries

本计划对应 PR 2，完成条件是 V2 公共契约、工作流状态推导、Mock/异步校验、线程边界与自动化测试均按规格可实现并可验证。以下内容明确不在本阶段及 PR 2 设计实施范围内：

- PR 3 的界面迁移、视觉改版或新增交互流程；
- JSON schema、工程文件保存/加载或 nlohmann/json 选型；
- 跨 revision 校验缓存复用；
- 对既有不可变计算任务的取消、失败重判或执行快照格式重构；
- 求解器、网格读取、结果后处理或 VTK 行为；
- `tasks.md` 生成和任何业务 C++ 实现。

## Phase Artifacts

- [research.md](./research.md)：Phase 0 决策、替代方案与结论。
- [data-model.md](./data-model.md)：公共视图、内部事实、印章、运行期模型和状态转换。
- [workflow-v2-contract.md](./contracts/workflow-v2-contract.md)：拟议公共接口、元类型、信号、兼容矩阵和非契约项。
- [quickstart.md](./quickstart.md)：后续实现阶段的独立构建、CTest 与场景验证指南。
