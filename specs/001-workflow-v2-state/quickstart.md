# Quickstart: Workflow V2 验证指南

## Purpose

本文说明 PR 2 实现后如何构建、运行自动化验收并核对关键行为。当前 Plan 阶段未修改 C++，因此以下是后续实现必须满足的可执行验证合同，不表示本次已运行这些新测试。

模型和接口细节参见：

- [data-model.md](data-model.md)
- [contracts/workflow-v2-contract.md](contracts/workflow-v2-contract.md)
- [research.md](research.md)

## 1. Prerequisites

正式验收基线：

- Windows x64
- Visual Studio 2017 v141
- C++14
- Qt 5.12.12 MSVC2017 64-bit
- CMake 3.18.2
- Debug configuration

Workflow V2 的 contracts、workflow 和 Mock 目标只依赖 Qt Core，不需要 SARibbon、Qt Widgets 或 VTK。

## 2. Windows Headless Full Validation

```bat
set "AEROSIM_QT_ROOT=C:\Qt\5.12.12\msvc2017_64"
set "AEROSIM_BUILD_GUI=OFF"
scripts\build-windows.cmd all Debug
```

`all` 必须在构建成功后运行 CTest。预期：

- contracts 与 workflow 不查找 Widgets、SARibbon 或 VTK；
- 新 Workflow V2 测试全部通过；
- `cli_mock_roundtrip` 继续通过；
- 未生成或读写任何正式 JSON 工程数据。

聚焦重跑：

```bat
ctest --test-dir build-windows -C Debug --output-on-failure ^
  -R "contracts_workflow_v2|workflow_|mock_workflow_v2|cli_mock_roundtrip"
```

## 3. Standalone Workflow Module Validation

Workflow 模块必须支持独立配置：

```bat
cmake -S modules\workflow -B build-workflow ^
  -G "Visual Studio 15 2017 Win64" ^
  "-DCMAKE_PREFIX_PATH=%AEROSIM_QT_ROOT%" ^
  -DAEROSIM_BUILD_TESTS=ON
cmake --build build-workflow --config Debug --parallel 6
ctest --test-dir build-workflow -C Debug --output-on-failure
```

预期只构建 `aerosim_contracts`、`aerosim_workflow` 及其测试目标，不构建 UI、Mock 应用、VTK 或求解器。

## 4. Auxiliary Linux/macOS Check

这是辅助检查，不替代 Windows 正式基线：

```sh
export AEROSIM_QT_ROOT=/path/to/Qt/5.12.12
export AEROSIM_BUILD_GUI=OFF
scripts/build-linux.sh all
ctest --test-dir build-linux --output-on-failure \
  -R 'contracts_workflow_v2|workflow_|mock_workflow_v2|cli_mock_roundtrip'
```

结果必须标记为辅助平台结果，不宣称 Linux/macOS 全栈已验证。

## 5. Expected CTest Targets

| CTest name | Validation responsibility |
| --- | --- |
| `contracts_public_headers_compile` | 新增 Workflow V2 类型与 `TaskPorts.h` 直接 include 自包含 |
| `contracts_stable_enum_token_manifest` | 旧 token 不变，V2 token 唯一且稳定 |
| `contracts_workflow_v2` | V2 默认值、token round-trip、`viewV2()` 默认 Unsupported、metatype 注册 |
| `workflow_state_derivation` | completion/validation/applicability 独立推导和草稿 overlay |
| `workflow_input_stamp` | 精确 stamp 匹配与纯展示属性排除 |
| `workflow_dependency_blockers` | 原子依赖、多 blocker、局部恢复、无关步骤隔离 |
| `workflow_action_authorization` | 新执行门控和修复/查看动作可用性 |
| `workflow_legacy_adapter` | 冻结映射优先级与单向 adapter |
| `workflow_snapshot_roundtrip` | 存储无关 facts 往返和恢复后派生状态重算 |
| `mock_workflow_v2` | 双 view 信号、异步乱序丢弃、`requestRun()` 权威门控 |
| `cli_mock_roundtrip` | 现有 CLI 装配和旧接口不回归 |
| `ui_window_integration` | 现有 MainWindow 仍使用 legacy view/action/navigation（GUI 构建时） |

## 6. Scenario Checklist

### 6.1 Draft and committed facts

1. 初始 scope 产生 `NotStarted + NotValidated`。
2. 对 `Completed + Valid` 步骤开始编辑，会话 view 显示 `InProgress`，factRevision 不变。
3. 取消或保存失败，完整恢复已提交 facts 和 revisions。
4. 保存新输入，恢复 `Completed`，有旧 stamp 时推导 `Stale`，无 validation fact 时推导 `NotValidated`。

### 6.2 Exact input stamp

每次只改变一项，验证原校验失效：

- scope；
- definitionVersion；
- parameter snapshot identity；
- parameter revision；
- resource identity；
- resource revision；
- verified fingerprint algorithm/digest/sizeBytes。

额外边界：

- fingerprint 相同但 revision 不同，仍为 Stale；
- fingerprint 未提供/未 verified，不得跳过 revision；
- 网格颜色、透明度、可见性和 displayScale 不进 stamp，不使校验失效。

### 6.3 Async out-of-order results

1. 使用确定性顺序或固定种子交付至少 100 组新旧结果，不使用 sleep 伪造竞态。
2. 只有当前 generation 且 stamp 完全匹配的结果被接受。
3. 丢弃的结果不改变 factRevision/viewRevision/issues/blockers/actions，不发 view 变化信号。

### 6.4 Dependency blockers

1. 只对 definition 显式声明的适用依赖产生 blocker。
2. 多个未满足原子要求分别产生 blocker。
3. 无关步骤失败不产生 blocker。
4. 仅恢复一条依赖时，只移除对应 blocker。
5. blocker 解除不改写 completion/validation fact；下游 stamp 仍旧时保持 Stale。

### 6.5 Action authorization

对需要当前有效校验的动作，分别使以下条件失败：

- applicable；
- Completed；
- Valid；
- blockers empty；
- stamp current。

任一失败必须拒绝新执行。Stale 时 browse/navigate/edit/save/revalidate 仍可用。`requestRun()` 必须重新授权；拒绝时不发 TaskCommand。

### 6.6 Legacy compatibility

完整覆盖：

- `Completed + Valid + blocker` → Blocked；
- `Completed + Stale` → NeedsValidation；
- `Completed + Invalid` → NeedsValidation；
- `Completed + Valid` → Complete；
- `InProgress + Valid` → Pending；
- `Completed + NotValidated` → Pending；
- non-applicable → Pending。

新旧 views 必须共享 scope/viewRevision，且 adapter 不得反写 facts。

### 6.7 Snapshot round-trip

1. 至少 100 次存储无关往返，保持 committed facts、issues、stamp、definitionVersion、factRevision 和时间。
2. snapshot 不包含 Stale、blockers、actions、nextSuggestion、draft 或 pending validations。
3. 恢复后使用当前 definition/inputs/dependencies 重算派生状态。
4. 不创建 JSON fixture，不读写工程文件。

## 7. Failure Interpretation

- contracts 失败：公共形状、token、include 或默认兼容行为回归；
- workflow 失败：状态语义、revision 或快照不变量回归；
- Mock 失败：双信号、乱序结果或 requestRun 装配回归；
- UI 失败：legacy view/action/navigation 兼容回归；
- 其他平台通过但 Windows Debug 未验证：不得宣称正式基线通过。
