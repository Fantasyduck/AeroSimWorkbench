# Data Model: Workflow V2 工作流状态模型

## 1. 模型边界

Workflow V2 分为三类数据：

1. **公共只读视图**：位于 `contracts/`，供 UI、CLI 和其他模块消费。
2. **工作流权威事实**：位于 `modules/workflow/`，是 V2 和 legacy 视图的唯一状态源。
3. **派生状态与运行期协调数据**：位于 `modules/workflow/`，可随当前定义、输入和依赖重算，不进入 facts snapshot。

对应参数、网格、State 和 Task 的真实实体仍由各自模块拥有。Workflow 仅保留状态推导所需的轻量引用和印章。

## 2. 公共只读视图

### 2.1 CompletionState

| Value | 语义 | 权威性 |
| --- | --- | --- |
| `NotStarted` | 尚无已提交工作 | 可作为 committed fact |
| `InProgress` | 当前工作未完成 | 可来自 committed partial fact，或由编辑草稿 overlay 显示 |
| `Completed` | 最近一次已提交配置完成 | committed fact，不代表当前校验有效 |

### 2.2 ValidationState

| Value | 推导条件 |
| --- | --- |
| `NotValidated` | 没有可用的历史 validation fact，或 legacy 数据缺少可用输入印章 |
| `Valid` | 最近 validation fact 为 passed，且精确输入印章与当前输入完全匹配 |
| `Invalid` | 最近 validation fact 为 failed，且印章仍与当前输入完全匹配 |
| `Stale` | 存在历史 validation fact，但其任一精确印章项与当前输入不匹配 |

`ValidationState` 是当前视图的派生结果，不直接保存在 snapshot 中。Snapshot 保存的是最近 validation fact 及它的印章。

### 2.3 DependencyBlockerView

| Field | Type | 语义 |
| --- | --- | --- |
| `dependencyStepId` | `QString` | 导致阻塞的上游步骤 |
| `code` | `QString` | 稳定未满足条件代码；在同一步骤内可与上游 ID 组成稳定 blocker 身份 |
| `message` | `QString` | 面向用户的修复说明 |
| `targetPageId` | `QString` | 与当前 UI 导航契约兼容的修复页面 |

Blocker 不是 fact。它与 completion/validation 分开表达，所以 `Completed + Valid + blockers[]` 是有效组合。

### 2.4 WorkflowStepViewV2

| Field | Type | 语义 |
| --- | --- | --- |
| `stepId` | `QString` | 定义内稳定步骤 ID |
| `scope` | `ScopeRef` | 步骤所属工程/工况作用域 |
| `applicable` | `bool` | 当前定义、求解模式与 scope 是否使步骤适用 |
| `applicabilityRuleId` | `QString` | 适用性判断的规则来源；非空时必须稳定 |
| `completion` | `CompletionState` | 已提交 fact 与当前 draft overlay 合成后的会话视图 |
| `validation` | `ValidationState` | 按当前 input stamp 推导 |
| `blockers` | `QVector<DependencyBlockerView>` | 当前所有未满足的显式依赖 |
| `summary` | `QString` | 用户可读摘要 |
| `targetPageId` | `QString` | 该步骤的导航目标 |
| `factRevision` | `quint64` | 最近一次修改该步骤 committed facts 时的作用域 fact revision |

### 2.5 WorkflowViewV2

| Field | Type | 语义 |
| --- | --- | --- |
| `scope` | `ScopeRef` | 快照作用域 |
| `definitionVersion` | `QString` | 生成该 view 的当前工作流定义版本 |
| `viewRevision` | `quint64` | 作用域内单调的可观察 view 版本 |
| `steps` | `QVector<WorkflowStepViewV2>` | 当前步骤视图 |
| `actions` | `QVector<WorkflowActionView>` | 服务端派生的动作授权与原因 |
| `issues` | `QVector<ValidationIssue>` | 当前可展示问题 |
| `nextSuggestion` | `QString` | 从当前视图推导的建议，非 fact |

## 3. 模块内部定义模型

### 3.1 WorkflowDefinition

```text
WorkflowDefinition
├─ definitionVersion: QString (non-empty)
├─ steps: QVector<WorkflowStepDefinition> (stepId unique)
└─ actions: QVector<WorkflowActionRule> (actionId unique)
```

### 3.2 WorkflowStepDefinition

```text
WorkflowStepDefinition
├─ stepId
├─ summary
├─ targetPageId
├─ applicabilityRuleId
├─ applicable predicate descriptor
└─ dependencies: QVector<DependencyRequirement>
```

本 PR 的 definition 是 workflow 模块内部值模型，不定义 JSON 文件格式或外部 DSL。Mock 可直接构造值对象。

### 3.3 DependencyRequirement

```text
DependencyRequirement
├─ requirementId: QString (unique within dependent step)
├─ dependentStepId
├─ dependencyStepId
├─ conditionKind:
│  ├─ CompletionSatisfied
│  ├─ CurrentValidationValid
│  └─ InputVersionMatches
├─ code
├─ message
├─ targetPageId
└─ applicability descriptor
```

每条原子 requirement 最多产生一个 blocker。多个未满足条件使用多条 requirement，不把多条件压入一个无法诊断的布尔表达式。

### 3.4 WorkflowActionRule

```text
WorkflowActionRule
├─ actionId
├─ targetPageId
├─ requiredStepIds
└─ requiresCurrentValidation: bool
```

`requiresCurrentValidation=false` 用于 browse/navigate/edit/save/revalidate 等修复与查看类动作。`true` 用于新单次执行、新批处理执行及其他明确需要当前有效输入的动作。

## 4. 权威事实模型

### 4.1 WorkflowFactsSnapshot

```text
WorkflowFactsSnapshot
├─ scope: ScopeRef
├─ definitionVersion: QString
├─ factRevision: quint64
├─ capturedAt: QDateTime
└─ steps: QVector<WorkflowStepFacts>
```

验证规则：

- scope 必须有可用 projectId 和当前业务所需的工况身份；
- definitionVersion 非空；
- factRevision 不小于所有 step factRevision；
- stepId 在 snapshot 内唯一；
- 任一 validation fact/stamp 不合法时整个 restore 失败，不部分应用。

### 4.2 WorkflowStepFacts

```text
WorkflowStepFacts
├─ stepId
├─ committedCompletion: CompletionState
├─ hasValidationFact: bool
├─ validationFact: ValidationFact
├─ factRevision: quint64
└─ updatedAt: QDateTime
```

`committedCompletion` 可表达已提交的部分进度，但 UI 输入框的未保存变化不得直接写入该字段。

### 4.3 ValidationFact

```text
ValidationFact
├─ outcome: Passed | Failed
├─ issues: QVector<ValidationIssue>
├─ inputStamp: ValidationInputStamp
└─ validatedAt: QDateTime
```

`Passed` 和 `Failed` 是当时对给定 stamp 的历史事实，不直接等于当前 `Valid` 或 `Invalid`。当 stamp 过期时，两种 outcome 均推导为 `Stale`。

## 5. 精确校验输入印章

### 5.1 ValidationInputStamp

```text
ValidationInputStamp
├─ scope: ScopeRef
├─ definitionVersion: QString
├─ parameter: ParameterInputStamp
└─ resources: QVector<ResourceInputStamp>
```

### 5.2 ParameterInputStamp

```text
ParameterInputStamp
├─ snapshotId
├─ scope
└─ revision (> 0 for a current validated input)
```

它与已有 `ParameterSnapshotRef` 语义对齐，匹配 snapshotId、scope 全部身份字段和 revision。

### 5.3 ResourceInputStamp

```text
ResourceInputStamp
├─ projectId
├─ resourceId
├─ revision (> 0)
├─ hasVerifiedFingerprint: bool
└─ fingerprint (when participated and verified)
   ├─ algorithm
   ├─ digest
   └─ sizeBytes
```

不记录/不比较 location、modifiedAt、网格颜色、透明度、可见性或 displayScale。

### 5.4 Stamp 精确匹配不变量

1. scope 所有身份字段必须匹配；
2. definitionVersion 必须匹配；
3. 参数 snapshotId、scope 与 revision 必须匹配；
4. 资源集合按 `(projectId, resourceId)` 归一化，身份必须唯一，集合增删即不匹配；
5. 每个资源 revision 必须匹配；
6. stamp 记录 verified fingerprint 时，当前 fingerprint 也必须 verified，且 algorithm/digest/sizeBytes 必须匹配；
7. fingerprint 相同不能忽略 revision 差异；
8. 缺失或未 verified fingerprint 不证明内容相同，不允许跨 revision 复用。

## 6. 运行期协调模型

### 6.1 EditingDraftOverlay

```text
EditingDraftOverlay
├─ scope
├─ stepId
├─ baseFactRevision
└─ sessionId
```

Overlay 存在时，当前会话 view 将 completion 显示为 `InProgress`。它不进入 snapshot，不改写 validation fact，不递增 factRevision。

### 6.2 PendingValidation

```text
PendingValidation
├─ requestId
├─ scope
├─ stepId
├─ generation
└─ inputStamp
```

同一 scope/step 的每次新校验请求递增 generation，只有当前 generation 的结果可继续进入 stamp 比较。

### 6.3 ValidationCompletion

```text
ValidationCompletion
├─ requestId
├─ scope
├─ stepId
├─ generation
├─ inputStamp
├─ outcome
├─ issues
└─ completedAt
```

如果该消息通过跨线程 queued signal 交付，必须作为模块级元类型按值注册；它不进全局 contracts。

## 7. 关系和数据流

```text
WorkflowDefinition + CurrentInputs + WorkflowFactsStore
                    + EditingDraftOverlay
                             |
                             v
                    WorkflowStateDeriver
                      /       |       \
                     v        v        v
              Validation   Dependency  Action
                state       blockers   authorization
                     \        |        /
                      v       v       v
                       WorkflowViewV2
                             |
                             v
                    LegacyWorkflowAdapter
                             |
                             v
                       WorkflowView
```

Snapshot 只从 `WorkflowFactsStore` 导出。Restore 只替换 committed facts，然后重新走上述推导流程。

## 8. 状态转换

| Event | Committed completion | Draft overlay | Validation fact | Derived validation | factRevision | viewRevision |
| --- | --- | --- | --- | --- | --- | --- |
| 新 scope | NotStarted | none | none | NotValidated | initial | initial view |
| 开始直接编辑 | unchanged | active | unchanged | 按已提交输入推导 | unchanged | increment if view changed |
| 取消/保存失败 | unchanged | removed | unchanged | 恢复已提交 view | unchanged | increment if view changed |
| 保存成功 | Completed | removed | retained | NotValidated or Stale | increment | increment |
| 校验通过且印章当前 | unchanged | unchanged | Passed + stamp | Valid | increment | increment |
| 校验失败且印章当前 | unchanged | unchanged | Failed + issues + stamp | Invalid | increment | increment |
| 上游输入变化 | unchanged | unchanged | retained | Stale when stamp mismatches | unchanged | increment if visible state changed |
| blocker 出现/解除 | unchanged | unchanged | unchanged | independently derived | unchanged | increment if view changed |
| 过期异步结果 | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged; no view signal |
| snapshot restore | restored | cleared | restored | recomputed | preserve snapshot revision | publish a newer view revision |

## 9. 依赖推导规则

`DependencyEvaluator` 对当前步骤执行：

1. 只枚举当前 definition 显式声明的 requirements；
2. 排除当前不适用的 requirement；
3. 按原子 conditionKind 读取上游当前推导状态/版本；
4. 每个未满足 requirement 产生一个 blocker；
5. 依赖恢复时重跑同一纯函数，仅已满足 requirement 对应的 blocker 消失；
6. 不修改下游 completion 或 validation fact；
7. 不查看未声明的无关步骤。

## 10. 动作授权规则

对 `requiresCurrentValidation=true` 的动作，关联每个步骤必须同时满足：

- applicable == true；
- 会话 completion == Completed；
- derived validation == Valid；
- blockers 为空；
- validation stamp 与当前 input stamp 仍精确匹配。

任一失败即返回 disabled action 和稳定 reason/targetPageId。`requestRun()` 必须在发出 TaskCommand 前重新执行授权，不信任较早视图的 enabled。

## 11. Snapshot Restore 规则

1. 验证整个 snapshot 的 scope、唯一性、revision 和 stamp 结构；
2. 验证成功前不修改现有 facts；
3. 原子替换 committed facts；
4. 清除 draft overlays 和 pending validation generations；
5. 保留 snapshot factRevision/step factRevision；
6. 使用当前 definition、inputs 和 dependencies 重算 applicable、validation state、blockers、actions 和 nextSuggestion；
7. 当前 definition 新增的步骤以 NotStarted/NotValidated 出现；
8. 快照中已不属于当前 definition 的历史 step facts 可保留于快照审计数据，但不进入当前活动 view；
9. 发布一个新的 viewRevision。

## 12. Legacy 视图映射

| Priority | V2 condition | Legacy StepState |
| --- | --- | --- |
| 1 | blockers non-empty | Blocked |
| 2 | no blockers and validation is Stale or Invalid | NeedsValidation |
| 3 | no blockers and completion is Completed and validation is Valid | Complete |
| 4 | all remaining combinations, including non-applicable | Pending |

Legacy adapter 复制 scope、viewRevision、actions、issues、nextSuggestion、summary 和 targetPageId。该映射单向且无状态，禁止从 legacy StepState 恢复 V2 facts。
