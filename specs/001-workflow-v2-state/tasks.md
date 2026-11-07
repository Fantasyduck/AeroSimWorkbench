---

description: "Workflow V2 工作流状态模型升级正式实施任务清单"
---

# Tasks: Workflow V2 工作流状态模型升级

**Input**: `specs/001-workflow-v2-state/` 下已完成的 `spec.md`、`plan.md`、`research.md`、`data-model.md`、`contracts/workflow-v2-contract.md` 与 `quickstart.md`

**Implementation scope**: 原《公共数据契约升级设计与实施计划》的 **PR 2**：workflow facts、`WorkflowViewV2`、双信号、legacy adapter、Mock 与自动化测试。禁止扩展到正式 JSON 持久化、StateConfig/BatchPlan、Task/Attempt、求解器、网格/结果、Ribbon 或工作区 UI 重构。

**Test policy**: 规格 FR-021/SC-005 明确要求自动化验收，因此每个故事先创建能失败的具体测试，再实现生产代码。测试使用 C++14、Qt 5.12.12 Core、`QCoreApplication`/lambda/事件投递和 CTest，不新增 Qt Test 或 JSON 依赖。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 前置任务完成后可与同批标记任务并行，且不修改同一文件。
- **[Story]**: 映射到 `spec.md` 的 US1～US5；Setup、Foundational 和 Polish 不加故事标签。
- 每项任务均在描述中列出修改目标、前置依赖、完成条件与对应测试。

## Branch and PR Boundary Audit

- 当前分支为 `chore/adopt-sdd`，`main@3f6e156` 与 `origin/main@3f6e156` 都是其祖先；当前分支相对 `main` 为 `0 behind / 4 ahead`。
- 4 个提交依次为 Spec Kit 初始化、宪法、Workflow V2 Specify、Clarify/Plan 文档；`tasks.md` 及本轮一致性修订尚未提交。该状态只是当前审查快照，后续 Git 操作必须人工重新核对。
- **必须拆 PR**：当前分支应作为 SDD 初始化与 Workflow V2 设计文档 PR；Workflow V2 C++ 实施应在该文档 PR 合并后，从更新后的 `main` 新建 PR 2 实施分支。不要在当前 SDD 分支直接堆叠实现提交。
- 人工 Git 步骤：提交本功能全部 SDD 文档 → 推送当前文档分支 → 创建并审查 SDD PR → 合并到 `main` → 拉取/快进本地 `main` → 从最新 `main` 创建独立 Workflow V2 PR 2 分支。本文不自动执行或声称完成任一步。
- 冻结编号为：PR 0 设计与测试框架（已在 `main`）、PR 1 AccessResult/V2 Ref/资源与 token（已在 `main`）、PR 2 Workflow V2（本清单）、PR 3 StateConfig/BatchPlan append-only revision 与 execution snapshot。UI V2 消费迁移未被编号为 PR 3，必须另行评审，且不属于本清单。

---

## Phase 1: Setup and Interface Review Gate

**Purpose**: 在任何公共头或 QObject 接口变更前完成宪法要求的接口评审，并固定实施分支边界。

- [ ] T001 在 `specs/001-workflow-v2-state/contracts/workflow-v2-contract.md` 由负责公共契约的维护者人工记录接口评审结论、评审人/日期、调用方、源码兼容范围、QObject/vtable ABI 不承诺、线程归属、所有权、生命周期和迁移约束；前置：SDD 文档已独立提交、推送、审查并合并，且 PR 2 分支从最新 `main` 创建；完成：评审明确批准 `viewV2()` 非纯虚默认 Unsupported、V2 值信号和单一状态源后才允许 T002；测试：人工核对宪法 II/III 与契约第 3～9 节均有结论，任何自动化或文档编辑不得代替批准。

**Checkpoint**: T001 是公共契约修改的强制门禁；未批准时其余实现任务全部阻塞。

---

## Phase 2: Foundational — Public Contract and Shared Core

**Purpose**: 先冻结公共 V2 形状、token、增量服务端口和无 GUI 工作流模块骨架；所有用户故事依赖本阶段。

**⚠️ File ownership**: T002～T007 必须由同一“公共契约”实施序列串行完成；不得让多个智能体同时修改 `WorkflowModels.h`、`EnumTokens.*`、`TaskPorts.*`、`RegisterMetaTypes.cpp` 或 contracts CMake。

- [ ] T002 在 `contracts/tests/WorkflowV2ContractTests.cpp`、`contracts/tests/StableEnumTokenManifestTest.cpp`、`contracts/tests/compile/WorkflowModelsHeader.cpp`、`contracts/tests/compile/TaskPortsHeader.cpp` 和 `contracts/tests/CMakeLists.txt` 先加入并注册失败的公共契约测试；前置：T001；完成：测试明确覆盖 V2 默认值/轻量可复制形状、稳定 token/未知 token、旧派生类不覆写 `viewV2()` 仍可编译、默认 `workflow.v2.unsupported` 及元类型注册，不虚构 public struct 的 stepId 校验入口；测试：`contracts_public_headers_compile`、`contracts_stable_enum_token_manifest`、`contracts_workflow_v2` 已被 CTest 发现并在实现前按预期失败。
- [ ] T003 在 `contracts/include/aerosim/contracts/WorkflowModels.h` 增加 `CompletionState`、`ValidationState`、`DependencyBlockerView`、`WorkflowStepViewV2`、`WorkflowViewV2` 与 `Q_DECLARE_METATYPE(WorkflowViewV2)`；前置：T002；完成：严格保持 `stepId` 唯一、`applicable=false` 不是 completion、非空 `applicabilityRuleId` 可追溯、blocker 不改写 completion/validation、`factRevision` 只表示 committed facts、`viewRevision` 表示所有可观察变化，且旧模型字段不变；测试：`contracts_public_headers_compile` 与 `contracts_workflow_v2` 的模型/默认值用例通过。
- [ ] T004 在 `contracts/include/aerosim/contracts/EnumTokens.h` 和 `contracts/src/EnumTokens.cpp` 实现 V2 token 双向转换；前置：T003；完成：Completion 固定为 `not_started`/`in_progress`/`completed`，Validation 固定为 `not_validated`/`valid`/`invalid`/`stale`，未知 enum 返回空字符串、未知 token 返回 `AccessStatus::InvalidArgument`，所有旧 token 不变；测试：`contracts_stable_enum_token_manifest` 与 `contracts_workflow_v2` round-trip/unknown 用例通过。
- [ ] T005 在 `contracts/include/aerosim/contracts/TaskPorts.h`、新增 `contracts/src/TaskPorts.cpp` 和 `contracts/CMakeLists.txt` 增加并接入非纯虚 `AccessResult<WorkflowViewV2> viewV2(const ScopeRef&) const` 及按值 `workflowViewV2Changed(WorkflowViewV2)`；前置：T004；完成：`TaskPorts.h` 直接包含 `AccessResult.h`，默认实现返回 Unsupported、错误码 `workflow.v2.unsupported`、消息 `implementation does not provide Workflow V2`，旧方法/信号/槽签名原样保留，新增实现已进入 `aerosim_contracts`；测试：旧派生类编译测试、默认 Unsupported 测试和 public-header-first-include 测试通过。
- [ ] T006 在 `contracts/src/RegisterMetaTypes.cpp` 注册 `aerosim::WorkflowViewV2`，并在 `contracts/tests/WorkflowV2ContractTests.cpp` 用真实 queued connection 验证按值交付；前置：T005；完成：注册发生在可能建立跨线程连接前，不注册仅同步返回的 `AccessResult<WorkflowViewV2>`，不无用途地注册嵌套类型；测试：`contracts_workflow_v2` queued metatype 用例通过且无 Qt 未注册类型警告。
- [ ] T007 在 `contracts/CMakeLists.txt` 和 `contracts/tests/CMakeLists.txt` 完成公共契约目标与测试清单复核；前置：T006；完成：`aerosim_contracts` 仍只公开依赖 Qt5::Core，所有新增源和测试均已接入，现有 contracts 测试名称和旧消费者构建不回归；测试：独立 contracts 配置/构建后运行全部 `contracts_*` CTest，包含 `contracts_workflow_v2`。
- [ ] T008 在 `modules/workflow/include/aerosim/workflow/WorkflowDefinition.h`、`modules/workflow/include/aerosim/workflow/WorkflowFacts.h`、`modules/workflow/include/aerosim/workflow/WorkflowService.h` 和 `modules/workflow/tests/WorkflowDefinitionTests.cpp` 定义模块内部共享值模型/服务边界并先写定义校验测试；前置：T007；完成：测试覆盖 definitionVersion 非空、stepId/actionId 唯一、同一下游 requirementId 唯一、同一下游 `(dependencyStepId, code)` 唯一及稳定错误 `workflow.definition.duplicate_blocker_identity`，`DependencyRequirement` 至少支持三种 conditionKind，facts 不含 draft/pending/派生状态，公共 `contracts/` 不出现内部类型；测试：测试源已就绪，等待 T009 注册后必须先红后绿。
- [ ] T009 在新增 `modules/workflow/src/WorkflowDefinition.cpp`、`modules/workflow/CMakeLists.txt`、`modules/workflow/tests/CMakeLists.txt`、`modules/workflow/tests/WorkflowTestSupport.h` 和根 `CMakeLists.txt` 实现 definition 校验并建立首个真实 `aerosim_workflow`/`aerosim::workflow` 静态库与 `aerosim_workflow_tests` 聚合目标；前置：T008；完成：静态库包含真实 `WorkflowDefinition.cpp` 而非空/占位源，`workflow_definition_validation` 已注册并通过，独立 CMake 固定 C++14、Qt 5.12.12 EXACT Core、CMake 3.18.2 兼容语法和 MSVC `/utf-8`，不使用 C++17 API，顶层顺序 contracts→workflow→mocks，headless 不查找 Widgets/SARibbon/VTK；测试：独立 workflow 与顶层 headless 配置/构建成功，`ctest -R workflow_definition_validation` 通过。

**Checkpoint**: 公共契约和模块边界稳定；T010 以后不得再次改写公共 V2 语义，除非回到接口评审。

---

## Phase 3: User Story 1 — 保留完成事实并显示过期 (Priority: P1) 🎯 MVP

**Goal**: 将 committed completion、历史 validation fact 和编辑草稿分离，并按精确输入印章推导 NotValidated/Valid/Invalid/Stale。

**Independent Test**: 创建 `Completed + Valid` 步骤，改变上游已提交输入后仍为 `Completed + Stale`；直接编辑只叠加 `InProgress`，取消/失败不改 facts，保存后恢复 Completed 且不自动 Valid。

### Tests for User Story 1

- [ ] T010 [US1] 在 `modules/workflow/tests/WorkflowStateTests.cpp` 和 `modules/workflow/tests/CMakeLists.txt` 先写并注册状态事实与草稿测试；前置：T009；完成：覆盖新 scope `NotStarted + NotValidated`、Completed/Valid 独立、编辑 overlay 的 InProgress、取消/失败 facts/revision 不变、保存后 Completed+Stale、无历史校验时 NotValidated、accepted fact 与可观察 view revision 规则；测试：CTest 已发现 `workflow_state_derivation`，且在实现前按预期失败。
- [ ] T011 [US1] 在 `modules/workflow/tests/WorkflowInputStampTests.cpp` 和 `modules/workflow/tests/CMakeLists.txt` 先写并注册精确印章矩阵；前置：T010；完成：逐项覆盖 scope、definitionVersion、parameter snapshotId/scope/revision、resource identity/revision、verified fingerprint algorithm/digest/sizeBytes、集合增删/顺序/重复 identity，并覆盖相同 fingerprint 不覆盖 revision、未验证 fingerprint 不绕过、location/modifiedAt/MeshPresentation 不参与；测试：CTest 已发现 `workflow_input_stamp`，且在实现前按预期失败。

### Implementation for User Story 1

- [ ] T012 [US1] 在 `modules/workflow/include/aerosim/workflow/ValidationStampMatcher.h`、`modules/workflow/src/ValidationStampMatcher.cpp` 和 `modules/workflow/CMakeLists.txt` 实现并接入唯一精确印章归一化与比较器；前置：T011、T008；完成：资源按 `(projectId, resourceId)` 稳定排序、拒绝重复 identity、逐项执行 data-model 5.4 的八条不变量，且 Stale 推导/异步接受/动作授权后续只能复用此比较器；测试：已注册的 `workflow_input_stamp` 全矩阵通过。
- [ ] T013 [US1] 在 `modules/workflow/include/aerosim/workflow/WorkflowStateDeriver.h`、`modules/workflow/src/WorkflowStateDeriver.cpp` 和 `modules/workflow/CMakeLists.txt` 实现并接入 completion/validation/applicability 纯推导与 `EditingDraftOverlay`；前置：T012；完成：draft 不写 facts、不递增 factRevision，Passed/Failed 只在当前 stamp 上分别派生 Valid/Invalid，任何历史 fact 印章失配派生 Stale，legacy 无印章派生 NotValidated；测试：已注册的 `workflow_state_derivation` 与 `workflow_input_stamp` 通过。
- [ ] T014 [US1] 在 `modules/workflow/src/WorkflowService.cpp` 和 `modules/workflow/CMakeLists.txt` 实现并接入 scope facts store、开始/取消编辑、保存成功/失败及统一 viewRevision 发布基础；前置：T013；完成：accepted fact 单调递增 factRevision，可观察 draft/输入/派生变化单调递增 viewRevision，保存原子提交新版本与 Completed 但不自动 Valid，失败不部分写入；测试：`workflow_state_derivation` 的所有 US1 验收和 revision 用例通过。
- [ ] T015 [US1] 在 `modules/workflow/tests/CMakeLists.txt` 将 `workflow_state_derivation` 与 `workflow_input_stamp` 纳入 `aerosim_workflow_tests` 聚合依赖并完成 US1 checkpoint；前置：T014；完成：独立模块目标和顶层 headless 目标均能构建运行这两个测试且不链接 Widgets/VTK；测试：`ctest -R "workflow_state_derivation|workflow_input_stamp"` 通过。

**Checkpoint**: US1 可独立演示 Completed 与 Stale 分离，建议作为最小 MVP 验证点。

---

## Phase 4: User Story 2 — 阻止依赖无效校验的计算 (Priority: P1)

**Goal**: 只根据显式适用依赖生成原子 blockers，并由服务统一授权新执行，同时保留浏览、编辑、保存、重校验及既有任务语义。

**Independent Test**: 构造 Valid/NotValidated/Invalid/Stale、未完成、印章失配和多 blocker 矩阵；只有 applicable+Completed+Valid+无 blocker+当前印章允许新执行，修复动作始终允许，局部依赖恢复只移除对应 blocker。

### Tests for User Story 2

- [ ] T016 [US2] 在 `modules/workflow/tests/WorkflowDependencyBlockerTests.cpp` 和 `modules/workflow/tests/CMakeLists.txt` 先写并注册依赖测试；前置：T015；完成：覆盖三种 conditionKind、仅显式且适用的依赖、多 blocker 全部可见、无关步骤隔离、局部恢复只移除对应 blocker、blocker 变化不改下游 completion/validation fact、上游恢复后下游旧 stamp 仍 Stale，以及重复 `(dependencyStepId, code)` 已在 definition validation 阶段原子拒绝；测试：CTest 已发现 `workflow_dependency_blockers`，且在实现前按预期失败。
- [ ] T017 [US2] 在 `modules/workflow/tests/WorkflowActionAuthorizationTests.cpp` 和 `modules/workflow/tests/CMakeLists.txt` 先写并注册动作矩阵；前置：T016；完成：逐项否定 applicable/Completed/Valid/blockers empty/current stamp，验证新单次与通用批执行 action 被拒绝且有稳定 code/message/targetPageId，browse/navigate/edit/save/revalidate 与计划编辑类 Mock action 允许但不创建 BatchPlan 类型；工作流变 Stale 时断言不发 Stop/Cancel、既有 MockTaskService record 字段逐项不变、无使历史结果失效的现有事件，拒绝不发 Start；测试：CTest 已发现 `workflow_action_authorization`，且在实现前按预期失败。

### Implementation for User Story 2

- [ ] T018 [US2] 在 `modules/workflow/include/aerosim/workflow/DependencyEvaluator.h`、`modules/workflow/src/DependencyEvaluator.cpp` 和 `modules/workflow/CMakeLists.txt` 实现并接入原子依赖纯计算；前置：T017、T008；完成：每条未满足 requirement 产生一个含 dependencyStepId/code/message/targetPageId 的 blocker，恢复只消失对应 blocker，不查看未声明步骤且不写 facts；测试：已注册的 `workflow_dependency_blockers` 通过。
- [ ] T019 [US2] 在 `modules/workflow/include/aerosim/workflow/ActionAuthorizer.h`、`modules/workflow/src/ActionAuthorizer.cpp` 和 `modules/workflow/CMakeLists.txt` 实现并接入动作授权纯计算；前置：T018、T012；完成：`requiresCurrentValidation=true` 同时检查适用性、Completed、Valid、无 blocker和精确 stamp，false 类动作不因 Stale 整体锁定，失败生成现有导航字段可消费的原因；测试：已注册的 `workflow_action_authorization` 纯授权矩阵通过。
- [ ] T020 [US2] 在 `modules/workflow/src/WorkflowStateDeriver.cpp` 集成 `DependencyEvaluator` 与 `ActionAuthorizer`；前置：T018、T019；完成：completion、validation、blockers 分别推导，同一 view 同时保留合法组合，blocker 出现/解除只在视图变化时递增 viewRevision；测试：`workflow_state_derivation`、`workflow_dependency_blockers`、`workflow_action_authorization` 共同通过。
- [ ] T021 [US2] 在 `modules/workflow/src/WorkflowService.cpp` 实现 `requestRun()` 请求时重新授权；前置：T020；完成：不信任缓存 action.enabled，拒绝时 `allowed=false` 且不发 `taskCommandRequested`，允许时至多发一个旧 `TaskCommand`，当前 Stale 不取消或重判既有任务；测试：竞态前置输入变化、拒绝无命令和允许单命令用例通过。
- [ ] T022 [US2] 在 `modules/workflow/tests/CMakeLists.txt` 将 `workflow_dependency_blockers` 与 `workflow_action_authorization` 纳入 `aerosim_workflow_tests` 聚合依赖并完成 US2 checkpoint；前置：T021；完成：两个已注册测试可独立过滤运行且 headless 构建不增加 GUI 依赖；测试：`ctest -R "workflow_dependency_blockers|workflow_action_authorization"` 通过。

**Checkpoint**: US1+US2 形成可安全门控新执行的 P1 纵向切片；不得以当前工作流状态追溯修改已创建任务。

---

## Phase 5: User Story 3 — 忽略过期的异步校验结果 (Priority: P2)

**Goal**: 以 per-scope/step generation 与精确输入印章双门控异步结果，所有 facts 写入回到服务所属线程。

**Independent Test**: 以确定性顺序交付 A/B 新旧请求至少 100 组；只有当前 generation 且 stamp 完全匹配的结果被接受，其他结果不改变任何 revision/issues/blockers/actions 或业务信号。

### Tests for User Story 3

- [ ] T023 [US3] 在 `modules/workflow/tests/WorkflowAsyncValidationTests.cpp` 和 `modules/workflow/tests/CMakeLists.txt` 先写并注册异步协调测试；前置：T022；完成：覆盖 A→B/B→A、同 stamp 多 generation、scope/definition/parameter/resource/fingerprint 变化、重复/取消/销毁后结果、纯展示属性变化、接受成功/失败、至少 100 组固定种子乱序，禁止用 sleep 模拟竞态；测试：CTest 已发现 `workflow_async_validation`，且在实现前按预期失败。

### Implementation for User Story 3

- [ ] T024 [US3] 在 `modules/workflow/include/aerosim/workflow/ValidationTypes.h`、`modules/workflow/src/RegisterWorkflowMetaTypes.cpp` 和 `modules/workflow/CMakeLists.txt` 定义并接入按值 `ValidationCompletion` queued 元类型；前置：T023、T008；完成：消息含 requestId/scope/stepId/generation/inputStamp/outcome/issues/time，不进入全局 contracts，注册先于跨线程连接；`PendingValidation` 仅作为服务线程内部状态，不注册 Qt 元类型；测试：`workflow_async_validation` 的 queued value delivery 用例无警告通过。
- [ ] T025 [US3] 在 `modules/workflow/include/aerosim/workflow/ValidationCoordinator.h`、`modules/workflow/src/ValidationCoordinator.cpp` 和 `modules/workflow/CMakeLists.txt` 实现并接入 generation 生命周期与结果接受判定；前置：T024、T012；完成：每个 scope/step 新请求单调递增，先比较当前 generation 再调用唯一 stamp matcher，替代/取消/scope 销毁/shutdown 使旧请求失效，worker 不直接访问 facts；测试：全部乱序、重复、取消和销毁用例通过。
- [ ] T026 [US3] 在 `modules/workflow/src/WorkflowService.cpp` 集成 `ValidationCoordinator` 和服务线程提交边界；前置：T025；完成：跨线程使用 `Qt::QueuedConnection` 按值回到 facade 线程，只有该线程可修改 facts/revisions/发布 views，过期结果不递增 revision、不清问题/blocker/action、不发 view 信号，shutdown 防止晚到回调；测试：线程 affinity、无重复连接和 discarded-result-no-observable-change 用例通过。
- [ ] T027 [US3] 在 `modules/workflow/tests/CMakeLists.txt` 将已注册的 `workflow_async_validation` 纳入 `aerosim_workflow_tests` 聚合依赖并完成 US3 checkpoint；前置：T026；完成：测试可在 Qt Core event loop 下确定性执行，连续至少 100 组结果无波动；测试：重复运行 `ctest -R workflow_async_validation --repeat until-fail:100` 无失败。

**Checkpoint**: 异步专业校验仍可由外部 worker 提供，但 Workflow V2 已保证旧结果不能覆盖当前事实。

---

## Phase 6: User Story 4 — 保持现有界面和调用方兼容 (Priority: P3)

**Goal**: V2 facts 作为唯一状态源，同时向旧 UI/CLI 发布冻结映射的 `WorkflowView`，现有接口无需立即迁移。

**Independent Test**: 对同一 V2 view 验证 Blocked→NeedsValidation→Complete→Pending 优先级、新旧 view 的 scope/viewRevision 一致、旧派生类默认 Unsupported、旧 run/task event/navigation 路径继续工作。

### Tests for User Story 4

- [ ] T028 [P] [US4] 在 `modules/workflow/tests/WorkflowLegacyAdapterTests.cpp` 和 `modules/workflow/tests/CMakeLists.txt` 先写并注册完整映射矩阵；前置：T027；完成：覆盖 `Completed+Valid+blocker→Blocked`、Stale/Invalid→NeedsValidation、Completed+Valid→Complete、InProgress+Valid/Completed+NotValidated/non-applicable→Pending，验证 scope/viewRevision/actions/issues/summary/targetPageId 保留且 adapter 无反写 API；测试：CTest 已发现 `workflow_legacy_adapter`，且在实现前按预期失败。
- [ ] T029 [P] [US4] 在 `mocks/services/tests/MockWorkflowV2Tests.cpp`、新增 `mocks/services/tests/CMakeLists.txt` 和 `mocks/services/CMakeLists.txt` 先写并注册 Mock 兼容测试；前置：T027；完成：覆盖 V2 查询、同 revision 双信号、旧 task event 输入、可控 completion/validation/input/blocker/recovery/乱序结果、requestRun 权威门控，并明确不证明真实专业校验或求解；测试：CTest 已发现 `mock_workflow_v2` 且在实现前按预期失败，所改 mocks 文件不与 T028 的 workflow 文件重叠。

### Implementation for User Story 4

- [ ] T030 [US4] 在 `modules/workflow/include/aerosim/workflow/LegacyWorkflowAdapter.h`、`modules/workflow/src/LegacyWorkflowAdapter.cpp` 和 `modules/workflow/CMakeLists.txt` 实现并立即接入单向无状态 legacy adapter；前置：T028；完成：adapter 源在 T031 调用前已进入 `aerosim_workflow`，严格按 blocker、Stale/Invalid、Completed+Valid、fallback 顺序映射，沿用现有 scopeId 投影规则且不生成第二个工况身份；测试：已注册的 `workflow_legacy_adapter` 全矩阵通过且链接成功。
- [ ] T031 [US4] 在 `modules/workflow/src/WorkflowService.cpp` 实现 `view()`/`viewV2()` 同源查询和一次 revision 的双视图发布；前置：T030、T026；完成：先从 facts 生成一份 V2 再适配 legacy，二者 scope/viewRevision 一致，继续发布旧信号并发布 V2 值信号，不承诺跨接收者相对顺序，丢弃旧校验不发布业务变化；测试：双查询/双信号一致性和旧信号持续可用用例通过。
- [ ] T032 [US4] 在 `mocks/services/MockWorkflowService.h`、`mocks/services/MockWorkflowService.cpp` 和 `mocks/services/CMakeLists.txt` 将 Mock 改为复用正式 Workflow V2 推导/adapter 的可控占位服务并立即链接 `aerosim::workflow`；前置：T029、T031；完成：不维护第二套 legacy facts，可注入状态/输入/依赖/返回顺序，旧 API 与 `actionId == "run"` 保持可用，允许时最多发一个 TaskCommand，类注释/输出继续标明 Mock，测试运行前所需正式 workflow 符号已链接；测试：已注册的 `mock_workflow_v2` 通过。
- [ ] T033 [US4] 在 `mocks/services/tests/CMakeLists.txt` 和根 `CMakeLists.txt` 完成 Mock/CLI 顶层集成；前置：T032；完成：workflow 在 mocks 之前构建，CLI headless 不链接 Widgets，现有 `cli_mock_roundtrip` 测试名保留，Mock 测试目标无需等待后续任务补链接；测试：`mock_workflow_v2` 和 `cli_mock_roundtrip` 通过。
- [ ] T034 [US4] 在 `modules/ui/tests/MainWindowTest.cpp` 增加 legacy-only 兼容断言而不修改 UI 生产代码；前置：T033；完成：旧测试替身无需覆写 `viewV2()`，现有 run action、禁用原因、targetPageId/导航、重复 binding 不重复请求均保持，测试不要求 MainWindow 消费 V2；测试：GUI 可用时 `ui_window_integration` 通过。
- [ ] T035 [US4] 在 `modules/workflow/tests/CMakeLists.txt` 和 `mocks/services/tests/CMakeLists.txt` 将已注册的 legacy/Mock 测试纳入各自聚合目标并完成 US4 checkpoint；前置：T034；完成：legacy adapter 已归正式 workflow 模块、Mock 只复用不复制，所有测试目标在调用其源码后才执行；测试：`ctest -R "workflow_legacy_adapter|mock_workflow_v2|cli_mock_roundtrip|ui_window_integration"` 在适用构建中通过。

**Checkpoint**: PR 2 保持旧消费者可用；禁止在本阶段切换 MainWindow/Ribbon/工作区树到 V2。

---

## Phase 7: User Story 5 — 恢复工作流事实并重算派生状态 (Priority: P3)

**Goal**: 提供存储无关的 facts snapshot/restore，原子恢复 committed facts 后使用当前定义、输入和依赖重新推导所有派生状态。

**Independent Test**: 至少 100 次向空 facts store 的内存往返保持 committed facts/stamp/definitionVersion/revisions/timestamps；snapshot 不含 Stale/blockers/actions/nextSuggestion/draft/pending，恢复时旧输入不匹配仍重算 Stale；非空 store 拒绝 revision 回退，相同内容重复恢复幂等，恢复后的事实修改继续递增。

### Tests for User Story 5

- [ ] T036 [US5] 在 `modules/workflow/tests/WorkflowSnapshotTests.cpp` 和 `modules/workflow/tests/CMakeLists.txt` 先写并注册 snapshot 验收；前置：T035；完成：覆盖 scope/definition/revision/step 唯一/stamp 整体校验、100 次空 store 往返、旧 snapshot 覆盖高 revision 返回 Conflict、同 revision 同内容幂等且无信号、同 revision 不同内容 Conflict、更高 revision 原子恢复、所有失败零部分修改、清 draft/pending、新 definition 步骤初始状态、移除步骤不进活动 view、恢复后首次事实修改递增、当前输入/依赖重算及不出现 JSON/file IO；测试：CTest 已发现 `workflow_snapshot_roundtrip`，且在实现前按预期失败。

### Implementation for User Story 5

- [ ] T037 [US5] 在 `modules/workflow/include/aerosim/workflow/WorkflowSnapshot.h`、`modules/workflow/src/WorkflowSnapshot.cpp` 和 `modules/workflow/CMakeLists.txt` 实现并接入存储无关 `WorkflowFactsSnapshot` 验证与值往返；前置：T036、T008；完成：只含 scope、definitionVersion、factRevision、capturedAt、step committed facts/最近 validation fact/stamp/issues/revisions/times，要求 scope 有效、definitionVersion 非空、scope revision 不小于 step revision、stepId 唯一、任一坏 stamp 使整体失败，并提供 `AccessStatus::Conflict`/`workflow.snapshot.revision_conflict`；测试：snapshot 形状、非法输入、空 store round-trip 和冲突用例可链接执行。
- [ ] T038 [US5] 在 `modules/workflow/src/WorkflowService.cpp` 实现原子 snapshot 导出/恢复；前置：T037、T031；完成：空 store 接受合法快照并保留 revisions；非空 store 拒绝更低 revision及同 revision 不同内容；同 revision 同内容幂等且不改 revision/发信号；更高 revision 原子恢复并清 draft/pending；所有失败零部分修改；恢复后按当前 definition/inputs/dependencies 重算派生状态，首次事实修改从恢复最高 revision 继续递增；测试：`workflow_snapshot_roundtrip` 的全部恢复、Conflict、幂等、递增和 100 次往返通过。
- [ ] T039 [US5] 在 `modules/workflow/tests/CMakeLists.txt` 将已注册的 `workflow_snapshot_roundtrip` 纳入 `aerosim_workflow_tests` 聚合依赖并完成 US5 checkpoint；前置：T038；完成：目标不引入 nlohmann/json、不创建 JSON fixture、不读写工程文件；测试：`ctest -R workflow_snapshot_roundtrip` 通过并核对链接依赖仅含 contracts/Qt Core。

**Checkpoint**: 完成 PR 2 最小恢复语义；正式 JSON schema、迁移和存储 adapter 仍留待后续独立评审。

---

## Phase 8: Polish, Traceability, and Full Validation

**Purpose**: 补齐跨故事验收、构建证据与 PR 边界复核，不新增功能范围。

- [ ] T040 在 `modules/workflow/tests/WorkflowAcceptanceTests.cpp` 和 `modules/workflow/tests/CMakeLists.txt` 建立并注册 FR-001～FR-023 与五个用户故事全部 Given/When/Then 的自动化追踪表和缺口用例；前置：T039；完成：每个当前验收场景至少对应一个测试函数/已有 CTest 名称，明确 SC-001～SC-009 的 100 次矩阵，避免仅写“测试通过”；测试：CTest 已发现新增 `workflow_acceptance` 且覆盖缺口后通过。
- [ ] T041 在 `contracts/tests/WorkflowV2ContractTests.cpp` 和 `modules/workflow/tests/WorkflowAcceptanceTests.cpp` 增加公共契约轻量性、服务同线程同步查询、worker 不写 facts、shutdown/连接唯一性与性能回归断言；前置：T040；完成：不引入 QWidget/VTK/Qt Test/JSON，使用 1×/2×/4× 的 S/D/A/R 确定性夹具及 evaluator/matcher 操作计数验证 O(S+D+A+R)，禁止无关笛卡尔扫描，墙钟时间只作诊断；测试：`contracts_workflow_v2` 与 `workflow_acceptance` 通过且操作计数按规模线性增长。
- [ ] T042 在 `modules/workflow/tests/CMakeLists.txt` 和根 `CMakeLists.txt` 核对并接入最终测试名称/聚合目标，不无故修改 `scripts/build-windows.cmd`；前置：T041；完成：`aerosim_workflow_tests` 能构建全部 workflow tests，现有 Windows 脚本已能接受该 target 时保持原样，顶层 `all Debug` 构建后运行 CTest，过滤表达式覆盖 `contracts_workflow_v2|workflow_|mock_workflow_v2|cli_mock_roundtrip`，GUI 开启时保留 `ui_window_integration`；测试：CMake configure 与 `ctest -N` 显示 quickstart 约定的所有测试名称，并记录是否无需脚本改动。
- [ ] T043 在 `docs/phase1-validation.md` 追加 PR 2 的日期化验证记录；前置：T042；完成：记录 Windows VS2017 v141 x64 Debug 下 contracts、独立 workflow、顶层 headless、Mock/CLI、可用时 UI 的精确命令和结果，Linux/macOS 只标辅助，缺失环境如实标未验证；测试：执行 `scripts/build-windows.cmd aerosim_workflow_tests Debug`、`scripts/build-windows.cmd all Debug` 与相应 CTest，并把真实结果写入记录。
- [ ] T044 在 `specs/001-workflow-v2-state/checklists/requirements.md` 完成实施前范围审计；前置：T043；完成：确认无 StateConfig/BatchPlan/BatchRun/Task-Attempt V2、正式 JSON、跨 revision cache、UI 重构、求解/网格/结果代码，确认 PR 3 编号仍专属于 StateConfig/BatchPlan，列出所有未验证平台或外部接口评审阻塞；测试：`git diff --check`、变更路径审计和 PR 说明中的宪法合规检查通过。

---

## Dependencies & Execution Order

### Phase Dependencies

```text
SDD docs PR merged into main
        |
        v
T001 interface review gate
        |
        v
T002 → T003 → T004 → T005 → T006 → T007 → T008 → T009
        |
        v
US1: T010 → T011 → T012 → T013 → T014 → T015
        |
        v
US2: T016 → T017 → T018 → T019 → T020 → T021 → T022
        |
        v
US3: T023 → T024 → T025 → T026 → T027
        |
        v
US4: T028/T029 → T030 → T031 → T032 → T033 → T034 → T035
        |
        v
US5: T036 → T037 → T038 → T039
        |
        v
Polish: T040 → T041 → T042 → T043 → T044
```

### User Story Dependencies

- **US1 (P1)**: 依赖 Foundation；提供 completion/validation/stamp/draft 核心，是最小独立 MVP。
- **US2 (P1)**: 依赖 US1 的当前状态推导与 stamp matcher；依赖/授权测试可先并行编写，集成必须串行。
- **US3 (P2)**: 依赖 US1 facts/stamp 和 US2 授权视图，确保异步结果不会回退这些状态。
- **US4 (P3)**: adapter 测试与 Mock 测试可并行编写，但双信号/Mock 实现依赖完成的 V2 服务；不修改 UI 生产代码。
- **US5 (P3)**: 依赖完整 facts/deriver/service，才能验证恢复后重算所有派生状态。

### Shared-File Serialization Rules

| Shared file/area | Required serial owner/order | Forbidden parallel work |
| --- | --- | --- |
| `contracts/include/aerosim/contracts/WorkflowModels.h`、`EnumTokens.*`、`TaskPorts.*`、contracts CMake | T002→T007 单一契约序列 | 任何故事智能体不得同时编辑公共契约 |
| `modules/workflow/include/aerosim/workflow/WorkflowFacts.h` | T008 固定后只经接口评审变更 | US1/US3/US5 不得各自复制 stamp/facts 类型 |
| `modules/workflow/src/WorkflowStateDeriver.cpp` | T013→T020 | US1 与 US2 集成不得并行改同文件 |
| `modules/workflow/src/WorkflowService.cpp` | T014→T021→T026→T031→T038 | 所有服务集成任务必须按此顺序串行 |
| `modules/workflow/CMakeLists.txt`、`modules/workflow/tests/CMakeLists.txt` | T009→T010→T011→T012→T013→T014→T015→T016→T017→T018→T019→T022→T023→T024→T025→T027→T028→T030→T035→T036→T037→T039→T040→T042 | 每个测试任务先注册 CTest，每个实现任务立即接入对应源；同一 CMake 文件始终由单一集成人串行修改 |
| `mocks/services/MockWorkflowService.*` 与 mocks CMake | T029 测试→T032 实现→T033 接入 | 不与公共 contracts 或 UI 生产文件混改 |
| `modules/workflow/tests/WorkflowAcceptanceTests.cpp` | T040→T041 | 不得并行修改追踪表与性能/线程回归断言 |

---

## Parallel Execution Examples

### User Story 1

```text
串行：T010 state test/register → T011 stamp test/register → T012 matcher/source target → T013 deriver/source target → T014 service/source target → T015 checkpoint
原因：T010/T011 均修改 workflow tests CMake，任务整体不并行；测试源码内容可预先协作，但由单一 CMake owner 合入。
```

### User Story 2

```text
串行：T016 blocker test/register → T017 authorization test/register → T018 DependencyEvaluator/source target → T019 ActionAuthorizer/source target → T020 deriver → T021 service → T022 checkpoint
原因：测试和实现任务均需即时修改共享 CMake，避免同文件并行冲突。
```

### User Story 3

```text
串行为主：T023 tests → T024 message/metatype → T025 coordinator → T026 service → T027 CMake
原因：generation、线程提交与 service revisions 必须由一个集成序列保持一致。
```

### User Story 4

```text
并行：T028 adapter tests || T029 Mock tests
安全性：T028 修改 workflow tests CMake，T029 修改独立 mocks tests CMake；文件不重叠。
串行：T030 adapter + source target → T031 dual-view service → T032 Mock → T033 → T034 → T035 checkpoint
```

### User Story 5

```text
串行：T036 snapshot tests → T037 snapshot value model → T038 atomic service restore → T039 CMake
原因：restore 会修改共享 facts/service 文件，不能与其他故事集成并行。
```

---

## Implementation Strategy

### MVP First

1. 先合并当前 SDD 文档 PR，并从更新后的 `main` 创建 PR 2 实施分支。
2. 完成 T001 接口评审门禁。
3. 完成 Phase 2 公共契约与共享核心。
4. 完成 US1（T010～T015），独立验证 Completed/Stale/draft/stamp 语义。
5. 实际 PR 2 不应在 MVP checkpoint 合并；继续完成同为 P1 安全门禁的 US2，再完成 US3～US5 和兼容验收。

### Incremental Delivery Inside PR 2

1. 公共契约和 contracts tests 单独提交，保持可回退。
2. US1 状态事实/印章纯核心单独提交。
3. US2 dependency/action authorization 单独提交。
4. US3 async coordinator 与线程边界单独提交。
5. US4 legacy adapter/Mock/兼容测试单独提交。
6. US5 storage-neutral snapshot 单独提交。
7. 最终 CMake/全量验证与范围审计单独提交。

## Known Blockers and Stop Conditions

- T001 公共接口评审未批准时，禁止修改 `contracts/`。
- 当前 SDD 分支尚未合并 `main`；在其上直接实现会混合 SDD 初始化和 PR 2，应停止并先拆 PR。
- Windows VS2017 v141 x64 Debug/Qt 5.12.12 环境不可用时，T043 必须记录“未验证”，不得用 macOS/Linux 辅助结果冒充正式通过。
- 如果实现需要正式 JSON、StateConfig/BatchPlan、Task/Attempt V2、外部 workflow DSL、跨 revision cache 或 UI 生产改造，视为越界并停止，另开规格/评审；不得把这些工作计入 PR 2。
- PR 3 固定为 StateConfig/BatchPlan append-only revision 与 execution snapshot；任何人不得把 UI 改造塞入 PR 3，V2 UI 迁移需另行规格和评审。
