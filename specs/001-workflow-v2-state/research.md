# Phase 0 Research: Workflow V2 工作流状态模型升级

## Research Scope

本研究以 `main@3f6e156` 的公共契约和实现、已冻结《公共数据契约升级设计与实施计划》、已完成 Clarify 的 `spec.md` 和项目宪章为边界。研究仅解决 PR 2 所需的技术决策；不重新决定业务规则，不纳入 PR 3 的 StateConfig/BatchPlan，不引入 JSON 或文件 IO。

## Decision 1: 公共契约只暴露 V2 只读视图

**Decision**: 在 `WorkflowModels.h` 增量加入 `CompletionState`、`ValidationState`、`DependencyBlockerView`、`WorkflowStepViewV2` 和 `WorkflowViewV2`。公共视图保持 Qt Core 轻量值类型；工作流 facts、输入印章、定义、快照和异步结果 envelope 留在 `modules/workflow/`。

`WorkflowStepViewV2` 在冻结草案基础上增加 `applicabilityRuleId`，以满足“不适用必须可追溯规则来源”的已澄清要求。`DependencyBlockerView` 使用稳定 `code` 表达未满足条件，不在 PR 2 额外冻结公共依赖条件枚举。

**Rationale**: UI、CLI 和其他调用方只需要稳定的只读状态；内部 facts 与恢复形状由 workflow 模块拥有。这避免把未评审的持久化结构变成全局契约。

**Alternatives considered**:

- 把全部 facts 和 snapshot 放入 `contracts/`：扩大兼容面并提前冻结内部恢复模型，拒绝。
- 只增加枚举而不提供 V2 view：无法支持新调用方和双视图兼容，拒绝。
- 将依赖规则定义全部公共化：当前没有跨模块工作流定义端口，属于过早冻结，拒绝。

## Decision 2: `IWorkflowService` 采用非破坏式增量接口

**Decision**: 在现有 `IWorkflowService` 上增加非纯虚同步查询
`AccessResult<WorkflowViewV2> viewV2(const ScopeRef &scope) const`，基类默认返回
`AccessStatus::Unsupported` 和稳定错误码 `workflow.v2.unsupported`。同一 QObject 增加
`workflowViewV2Changed(WorkflowViewV2)` 信号。保留所有旧方法、信号和槽签名。

默认实现放在新的 `contracts/src/TaskPorts.cpp`，`TaskPorts.h` 显式包含
`AccessResult.h`，不依赖间接 include。

**Rationale**: `AccessResult` 是 PR 1 已确立的同步轻量查询错误载体，可区分不支持、未找到和真实空视图。非纯虚默认实现保证当前 Mock、UI 测试替身和旧实现继续编译。

**Alternatives considered**:

- 返回裸 `WorkflowViewV2`：空值无法表达 Unsupported，拒绝。
- 将 `viewV2` 设为纯虚：立即破坏所有旧派生类的源码兼容，拒绝。
- 新建第二个 QObject V2 服务：形成双状态源，违反冻结设计，拒绝。

## Decision 3: V2 facts 是唯一权威源，旧视图由无状态 adapter 产生

**Decision**: `modules/workflow/` 维护唯一 `WorkflowFactsStore`。`WorkflowStateDeriver`
从 facts、当前 definition、当前 inputs 和当前 dependencies 生成 V2 view；
`LegacyWorkflowAdapter` 再从该 V2 view 纯转换为旧 `WorkflowView`。Mock 和未来正式服务共用同一 adapter。

映射顺序固定为：

1. blockers 非空 → `Blocked`；
2. 否则 `Stale`/`Invalid` → `NeedsValidation`；
3. 否则 `Completed + Valid` → `Complete`；
4. 其余（包括不适用）→ `Pending`。

两个 view 使用同一 scope 和 `viewRevision`，但调用方不得依赖两个信号对不同接收者的相对到达顺序。

**Rationale**: 一次推导、一次 adapter 能保证新旧视图语义一致，并让当前 MainWindow 继续只消费旧视图。

**Alternatives considered**:

- 为 V2 和 legacy 分别保存事实：会分叉，拒绝。
- Mock 与正式服务各自复制映射：易产生优先级差异，拒绝。
- PR 2 直接切换 MainWindow 消费 V2：超出 UI 兼容范围，拒绝。

## Decision 4: 编辑草稿作为运行期视图叠加层

**Decision**: 已提交的 completion/validation facts 与 `EditingDraftOverlay` 分离。开始编辑只让当前会话 view 显示 `InProgress`，不递增 `factRevision`。取消或保存失败只移除 overlay。保存成功时原子提交新配置版本和 completion fact，移除 overlay，并重新推导 validation。

**Rationale**: 这直接满足取消和保存失败不改写已提交事实的澄清结论。

**Alternatives considered**:

- 输入框变化立即写 facts：无法无损取消，拒绝。
- 把草稿写入恢复快照：提前引入草稿持久化语义，拒绝。

## Decision 5: 精确输入印章使用唯一比较函数

**Decision**: `ValidationInputStamp` 记录 scope、definitionVersion、参数 snapshot 身份与 revision、以及实际参与校验的资源身份、revision 和已验证 fingerprint。资源集合按身份键归一化比较，不依赖 QVector 顺序。`ValidationStampMatcher` 是 Stale 推导、异步结果接受和动作授权的唯一匹配实现。

比较不受 location、modifiedAt 和 MeshPresentation 属性影响。记录 verified fingerprint 时比较 algorithm、digest 和 sizeBytes。相同 fingerprint 不覆盖 revision 差异；缺失/未 verified fingerprint 不证明内容相同。

**Rationale**: 共用一个纯比较器可防止三处规则漂移，并且与 PR 1 的 `ResourceRefV2`/`ContentFingerprint` 语义一致。

**Alternatives considered**:

- 直接比较整个 `ResourceRefV2`：location 和其他非校验字段会导致伪失效，拒绝。
- fingerprint 相同时忽略 revision：违反 Clarify 结论，拒绝。
- 跨 revision 缓存校验结果：本 PR 明确排除，拒绝。

## Decision 6: 异步校验同时比较 request generation 和输入印章

**Decision**: 每次校验请求捕获 requestId、scope/stepId、单调 generation 和精确输入印章。结果按值返回服务所属线程后，`ValidationCoordinator` 先验证它仍是该步骤的当前 generation，再重建当前 stamp 并执行精确匹配。

过期结果被静默丢弃：不递增 fact/view revision，不清除 issues，不改变 blockers/actions，不发布业务 view 倒退。可记录内部诊断，但不将其表达为业务状态。

**Rationale**: 只比较 stamp 无法防止同一输入版本上的较早并发请求覆盖较新请求；generation 填补该窗口。

**Alternatives considered**:

- 只比较 stamp：同 stamp 多请求仍可乱序，拒绝。
- 使用 sleep 或时间戳判断先后：不确定且测试不稳定，拒绝。

## Decision 7: 依赖阻塞使用原子声明规则

**Decision**: 内部 `DependencyRequirement` 以一条未满足条件对应一个 blocker 的粒度表达，至少支持 completion satisfied、current validation valid 和 input version matches。只评估当前步骤显式声明且适用的依赖。

一条要求重新满足时，纯推导会自然移除对应 blocker，不改写其他 blocker、下游 completion 或历史 validation fact。下游 ValidationState 继续独立按 stamp 判定。

**Rationale**: 原子规则天然满足多 blocker、局部恢复和无关步骤隔离。

**Alternatives considered**:

- 一个含多个模糊布尔字段的大依赖：无法稳定定位未满足条件，拒绝。
- 全局扫描任何失败步骤：会阻塞无关流程，拒绝。

## Decision 8: 动作授权在服务端每次重新推导

**Decision**: `ActionAuthorizer` 根据动作定义和关联步骤生成 `WorkflowActionView`。需要当前有效校验的新执行操作必须同时满足：applicable、Completed、Valid、无 blocker、stamp 与当前输入一致。`requestRun()` 不信任 UI 中已显示的 enabled，每次再执行相同授权。

browse、navigate、edit、save、revalidate 不因 Stale 禁用。批处理计划编辑属于 PR 3；PR 2 只保证通用动作规则能区分“编辑计划”和“提交执行”。已创建任务不受当前 Stale 追溯影响。

**Rationale**: UI 展示不是安全边界，服务端重新校验可防止绕过和旧 view 提交。

**Alternatives considered**:

- UI 自行判断：可被绕过，拒绝。
- Stale 时锁定整个页面：会阻断修复路径，拒绝。
- 当前 Stale 自动取消已有任务：违反不可变执行快照边界，拒绝。

## Decision 9: 快照是存储无关的 facts 往返，不是 JSON 契约

**Decision**: `WorkflowFactsSnapshot` 只包含 scope、definitionVersion、作用域级 factRevision、capturedAt 和每个步骤的 committed completion、最近 validation fact/stamp/issues、step factRevision 与时间。不包含 draft、pending validation、Stale、blockers、applicable、actions、nextSuggestion 或 viewRevision。

恢复先完整验证快照，再原子替换 facts；失败不得部分恢复。恢复后清理 draft/pending generation，保留快照 fact revisions，并使用当前 definition/inputs/dependencies 重算所有派生状态。

**Rationale**: 既满足 facts 可恢复语义，又不引入未定 JSON 库、文件 schema 和迁移协议。

**Alternatives considered**:

- PR 2 定义正式 JSON：超出 Clarify 边界且违反宪章的未定依赖约束，拒绝。
- 恢复快照中保存的 blockers/actions：会与当前输入分叉，拒绝。
- 不定义任何恢复语义：不满足已澄清的 PR 2 范围，拒绝。

## Decision 10: 服务所属线程是 facts 的唯一提交线程

**Decision**: `view()`/`viewV2()` 仅在服务与调用方同线程时做同步轻量查询。耗时专业校验可在 worker 执行，但结果必须以轻量值消息通过 `Qt::QueuedConnection` 送回服务所属线程，由该线程比较 stamp 并提交 facts。不为引擎加锁以支持任意跨线程同步调用。

`WorkflowViewV2` 在建立连接前通过 `registerContractMetaTypes()` 注册。只在内部排队消息实际使用时才为其声明并注册模块级元类型。`AccessResult<WorkflowViewV2>` 不注册，因为它只用于同线程同步查询。

**Rationale**: 单写线程模型比内部锁更简单、可测，也与当前 UI/service 同线程同步查询契约一致。

**Alternatives considered**:

- 让任意 worker 直接修改 facts：需要复杂锁且可产生视图竞态，拒绝。
- 为所有嵌套类型预先注册元类型：无实际 queued 用途，拒绝。
- 使用 `QSignalSpy`：需引入 Qt Test 新依赖；当前可用 QCoreApplication、lambda 和显式事件处理验收，不采用。

## Decision 11: 枚举 token 和测试分层在 PR 2 同步冻结

**Decision**: 新增稳定 token：

- CompletionState: `not_started`、`in_progress`、`completed`；
- ValidationState: `not_validated`、`valid`、`invalid`、`stale`。

未知枚举仍输出空字符串，未知 token 返回 `InvalidArgument`。测试分为 contracts 形状/token、workflow 纯状态引擎、Mock/装配三层，不使用 UI 测试代替服务规则验收。

**Rationale**: 冻结设计要求 V2 枚举在类型实现 PR 加入真实 round-trip 测试；分层测试可保持 headless 基线并快速定位失败。

**Alternatives considered**:

- 复用旧 StepState token：无法表达独立维度，拒绝。
- 存储 C++ ordinal：不稳定，拒绝。
- 只扩展 MainWindow 测试：绑定 Widgets/SARibbon 且不能覆盖 headless 规则，拒绝。

## Research Outcome

所有技术未知项已解决，无 `NEEDS CLARIFICATION` 保留项。方案可在 C++14、Qt 5.12.12、CMake 3.18.2 和 Windows VS2017 v141 x64 Debug 基线上实现，不需要 JSON、Qt Widgets、SARibbon 或 VTK。
