# Workflow V2 Public Contract Design

## Status and Scope

本文是 PR 2 的公共 C++/Qt Core 契约设计，供后续实现和接口评审使用，不是本次 Plan 阶段已修改的生产头文件。

**Review status**: Pending. 本文的修订、提交或合并均不等于公共接口已经批准。实施任务 T001 仍要求维护者人工记录评审人、日期、调用方影响、兼容性、线程、所有权、生命周期和迁移结论；批准前不得修改生产 `contracts/`。

设计保留以下现有契约不变：

- `IWorkflowService::view(const ScopeRef &) const`；
- `IWorkflowService::requestRun(const RunRequest &)`；
- `IWorkflowService::onTaskEvent(const TaskEvent &)`；
- `taskCommandRequested(TaskCommand)` 和 `workflowViewChanged(WorkflowView)`；
- `WorkflowView`、`WorkflowStepView`、`WorkflowActionView`、`StepState` 的字段和稳定 token。

仓库统一重编译，因此 PR 2 保证源码兼容，不承诺 QObject metaobject/vtable 的二进制 ABI 不变。

## 1. Public Model Additions

目标文件：`contracts/include/aerosim/contracts/WorkflowModels.h`

```cpp
enum class CompletionState {
    NotStarted,
    InProgress,
    Completed
};

enum class ValidationState {
    NotValidated,
    Valid,
    Invalid,
    Stale
};

struct DependencyBlockerView {
    QString dependencyStepId;
    QString code;
    QString message;
    QString targetPageId;
};

struct WorkflowStepViewV2 {
    QString stepId;
    ScopeRef scope;
    bool applicable = true;
    QString applicabilityRuleId;
    CompletionState completion = CompletionState::NotStarted;
    ValidationState validation = ValidationState::NotValidated;
    QVector<DependencyBlockerView> blockers;
    QString summary;
    QString targetPageId;
    quint64 factRevision = 0;
};

struct WorkflowViewV2 {
    ScopeRef scope;
    QString definitionVersion;
    quint64 viewRevision = 0;
    QVector<WorkflowStepViewV2> steps;
    QVector<WorkflowActionView> actions;
    QVector<ValidationIssue> issues;
    QString nextSuggestion;
};
```

### Model invariants

- `stepId` 在一个 view 内唯一；
- `applicable=false` 不是第四种 completion，且 `applicabilityRuleId` 必须能追溯判定来源；
- blocker 是派生数据，不改写 completion 或 validation fact；
- 同一个下游 step 的 blockers 中 `(dependencyStepId, code)` 唯一；重复身份由 workflow definition 校验以 `workflow.definition.duplicate_blocker_identity` 拒绝，不为此新增公共 blocker ID；
- `factRevision` 仅反映该步骤 committed facts 的最近修改；
- `viewRevision` 反映任何可观察视图变化，包括纯派生状态变化。

这些 public structs 是轻量被动值类型，不提供 definition/stepId 唯一性校验 API。stepId、actionId、requirement 和 blocker identity 的定义校验属于 `modules/workflow/`。

## 2. Stable Enum Tokens

目标文件：`EnumTokens.h/.cpp`

| Type | Enum | Stable token |
| --- | --- | --- |
| CompletionState | NotStarted | `not_started` |
| CompletionState | InProgress | `in_progress` |
| CompletionState | Completed | `completed` |
| ValidationState | NotValidated | `not_validated` |
| ValidationState | Valid | `valid` |
| ValidationState | Invalid | `invalid` |
| ValidationState | Stale | `stale` |

增加以下重载：

```cpp
QString enumToken(CompletionState value);
QString enumToken(ValidationState value);

AccessResult<CompletionState>
completionStateFromToken(const QString &token);

AccessResult<ValidationState>
validationStateFromToken(const QString &token);
```

未知 enum 值返回空 token；未知 token 返回 `AccessStatus::InvalidArgument`，不默认选择任一状态。旧 StepState token 不修改。

## 3. IWorkflowService Incremental Contract

目标文件：`contracts/include/aerosim/contracts/TaskPorts.h` 和新增
`contracts/src/TaskPorts.cpp`。

```cpp
class IWorkflowService : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~IWorkflowService() override = default;

    virtual WorkflowView view(const ScopeRef &scope) const = 0;
    virtual ValidationResult requestRun(const RunRequest &request) = 0;

    // Non-pure for source compatibility with existing implementations.
    virtual AccessResult<WorkflowViewV2>
    viewV2(const ScopeRef &scope) const;

public slots:
    virtual void onTaskEvent(const aerosim::TaskEvent &event) = 0;

signals:
    void taskCommandRequested(const aerosim::TaskCommand &command);
    void workflowViewChanged(const aerosim::WorkflowView &view);

    // Value message; queued delivery copies the registered metatype.
    void workflowViewV2Changed(aerosim::WorkflowViewV2 view);
};
```

默认 `viewV2()` 必须返回：

```text
status: AccessStatus::Unsupported
errorCode: workflow.v2.unsupported
message: implementation does not provide Workflow V2
```

`TaskPorts.h` 必须直接包含 `AccessResult.h`。`AccessResult<WorkflowViewV2>` 只能用于同线程同步查询，不通过 queued signal 传递，不注册为 Qt 元类型。

## 4. Qt Metatype Contract

目标文件：`WorkflowModels.h` 和 `RegisterMetaTypes.cpp`。

```cpp
Q_DECLARE_METATYPE(aerosim::WorkflowViewV2)
```

```cpp
qRegisterMetaType<aerosim::WorkflowViewV2>(
    "aerosim::WorkflowViewV2");
```

注册必须在建立可能跨线程的连接之前由既有 `registerContractMetaTypes()` 完成。嵌套的 step/blocker/enums 在不作为独立 queued 参数或 QVariant 使用时不额外注册。

## 5. Legacy Adapter Contract

目标文件：`modules/workflow/include/aerosim/workflow/LegacyWorkflowAdapter.h`。

```cpp
namespace aerosim {
namespace workflow {

WorkflowView toLegacyWorkflowView(
    const WorkflowViewV2 &view);

} // namespace workflow
} // namespace aerosim
```

映射是单向纯函数：

| Order | V2 condition | Legacy state |
| --- | --- | --- |
| 1 | blockers non-empty | `StepState::Blocked` |
| 2 | no blockers; validation is Stale or Invalid | `StepState::NeedsValidation` |
| 3 | no blockers; completion is Completed and validation is Valid | `StepState::Complete` |
| 4 | all other combinations | `StepState::Pending` |

Adapter 保留 scope、viewRevision、actions、issues、nextSuggestion、summary 和 targetPageId。Legacy `scopeId` 按当前兼容规则由步骤 scope 投影，不生成第二个工况身份。禁止从 legacy view 反向写回 V2 facts。

## 6. Signal Publication Invariants

对一次可观察视图变化，服务必须：

1. 从唯一 facts 和当前输入生成一份 `WorkflowViewV2`；
2. 通过 adapter 生成一份 legacy `WorkflowView`；
3. 使两份 view 共享同一 scope 和 viewRevision；
4. 继续发布旧 `workflowViewChanged`；
5. 同时发布新 `workflowViewV2Changed`。

不规定不同接收者观察到两个信号的相对顺序；调用方使用 scope/viewRevision 关联它们。过期异步校验被丢弃时不发布任何业务 view 变化信号。

## 7. Thread Contract

- `IWorkflowService` facade 与 facts store 归属创建它们的线程；
- `view()` 和 `viewV2()` 是同线程、轻量、无 IO 的同步查询；
- worker 校验结果通过 `Qt::QueuedConnection` 按值送回 facade 线程；
- 只有 facade 线程可提交 facts、递增 revisions 和发布 views；
- shutdown 时先断开/停止结果生产者；已排队的晚到结果必须因对象销毁、generation 过期或 stamp 不匹配而安全丢弃。

## 8. Request Authorization Contract

`WorkflowActionView::enabled` 仅供界面展示。`requestRun()` 必须在发出 `TaskCommand` 前再执行当前授权，至少检查：

- 必需步骤 applicable；
- completion == Completed；
- validation == Valid；
- blockers 为空；
- validation input stamp 与当前输入精确匹配。

拒绝时：

- 不发布 `taskCommandRequested`；
- `ValidationResult.allowed == false`；
- issues 包含稳定 code、用户可读 message 和可用 targetPageId。

允许时，Mock 最多发布一个现有 `TaskCommand`，且仍必须明确声明它不代表真实专业校验或求解成功。

## 9. Compatibility Matrix

| Consumer | PR 2 behavior |
| --- | --- |
| Existing MainWindow | 继续使用 legacy signal/view 和 `actionId == "run"`；无必需改动 |
| Existing CLI | 继续调用 `requestRun()`；服务内部重新授权 |
| Existing mock/test subclass | 不覆写 `viewV2()` 时继续编译并返回 Unsupported |
| V2-aware consumer | 同线程调用 `viewV2()`，或订阅已注册的 V2 signal |
| Task service | 仍只接收通过当前授权的旧 `TaskCommand` |
| Existing task/history/result | 不因当前 workflow Stale 自动取消、失败或失效 |

## 10. Explicit Non-Contracts

以下内容不在 PR 2 公共契约中冻结：

- Workflow facts/snapshot 的 JSON schema、文件名、路径和 schema migration；
- StateConfig、BatchPlan、BatchRun、Task/Attempt V2 形状；
- 网格读取、MeshPresentation、求解器和专业校验实现；
- 工作流定义的外部文件/DSL；
- 跨 revision 校验缓存复用；
- MainWindow 或 Ribbon/工作区树的 V2 UI 改造。
