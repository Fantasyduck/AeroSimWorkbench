# Feature Specification: Workflow V2 工作流状态模型升级

**Feature Branch**: `chore/adopt-sdd`

**Created**: 2026-11-06

**Status**: Draft

**Input**: User description: "为 AeroSimWorkbench 定义 Workflow V2 工作流状态模型升级（原公共数据契约实施计划 PR 2），仅规定功能需求、使用场景、边界条件和可验收标准。"

## Clarifications

### Session 2026-11-06

- Q: 用户直接重新编辑一个已完成步骤、但尚未再次保存时，该步骤的 CompletionState 应如何变化？ → A: 直接编辑会话显示 InProgress；保存后恢复 Completed 但不自动恢复 Valid；取消或保存失败不改变已提交事实，仅上游变化时保留 Completed 并推导 Stale。

### Session 2026-11-07

- Q: 当步骤为 Completed + Stale 时，系统应采用哪种业务动作门控规则？ → A: 只阻止新计算执行和其他明确依赖当前有效校验的动作；允许浏览、导航、编辑、保存和重新校验，已创建任务继续按不可变执行快照处理。
- Q: 当校验输入的 revision、fingerprint 或 definitionVersion 发生变化时，应采用哪条统一失效规则？ → A: 使用精确输入印章；所有实际参与校验的已记录印章都必须匹配，任一不匹配即失效，相同 fingerprint 不能覆盖 revision 差异。
- Q: 依赖阻塞项应在什么条件下出现，并在什么条件下恢复？ → A: 按工作流定义中每条适用依赖的完成、当前校验有效性和输入版本要求分别推导；要求重新满足后仅移除对应 blocker，不改写下游事实。
- Q: PR 2 对工作流事实的保存与恢复应承担到哪一层边界？ → A: 包含存储无关的工作流事实快照、恢复语义和往返验收；正式工程 JSON schema、文件 IO、迁移和存储适配器留待后续评审。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - 保留完成事实并显示过期 (Priority: P1)

作为配置人员，我在已完成步骤所依赖的上游输入发生变化后，仍能看到该步骤最近一次已提交配置已经完成，同时清楚得知原校验已过期，而不会把“做过”与“当前仍有效”混为一个状态。直接重新编辑该步骤时，当前编辑会话另行显示 InProgress。

**Why this priority**: 这是本次升级的核心价值，直接决定用户能否正确理解已完成配置的当前可用性。

**Independent Test**: 创建一个已完成且校验有效的步骤，修改其校验所依赖的上游已提交输入，验证完成状态保持为 Completed，校验状态变为 Stale。

**Acceptance Scenarios**:

1. **Given** 步骤已完成且针对当前输入校验通过，**When** 只有其依赖的上游输入发生变化，**Then** 步骤同时显示 Completed 和 Stale。
2. **Given** 步骤有一份已提交的 Completed 配置，**When** 用户直接编辑该步骤但尚未保存，**Then** 当前编辑会话显示 InProgress，最近一次已提交的配置事实、版本和校验结果保持不变。
3. **Given** 已完成步骤处于直接编辑会话，**When** 用户取消编辑或保存失败，**Then** 工作流恢复展示最近已提交状态，且不修改其完成事实、版本、校验事实或输入印章。
4. **Given** 已完成步骤的编辑会话保存成功，**When** 新的已提交输入与上次校验印章不同，**Then** 步骤恢复 Completed 并显示 Stale，不自动恢复 Valid。
5. **Given** 步骤已完成但从未校验，**When** 用户直接编辑并成功保存，**Then** 步骤恢复 Completed，校验状态为 NotValidated，不伪造 Stale 或 Valid。

---

### User Story 2 - 阻止依赖无效校验的计算 (Priority: P1)

作为计算人员，我在准备单次或批量计算时，希望任何依赖当前有效校验的操作都会在校验过期、未校验、校验失败或存在依赖阻塞时被禁止，并告知我原因及应返回的页面。

**Why this priority**: 该能力直接防止使用已过期或无效配置启动计算，是安全执行的必要前置条件。

**Independent Test**: 分别构造 Valid、NotValidated、Invalid 和 Stale 的已完成步骤，验证只有校验有效且无依赖阻塞的计算操作可执行。

**Acceptance Scenarios**:

1. **Given** 计算所依赖的步骤为 Completed 但校验为 Stale，**When** 用户尝试启动计算，**Then** 计算不得启动，界面提供过期原因和可定位的修复页面。
2. **Given** 所有必需步骤都是 Completed、Valid、无阻塞项且校验印章与当前输入版本一致，**When** 用户请求启动计算，**Then** 工作流允许该操作继续。
3. **Given** 步骤本身为 Completed 且 Valid，但存在上游阻塞项，**When** 用户查看计算操作，**Then** 操作被禁止且列出所有导致阻塞的上游步骤。
4. **Given** 步骤为 Completed 且 Stale，**When** 用户浏览页面、导航到问题位置、编辑或保存配置、或发起重新校验，**Then** 这些修复与查看动作仍可使用。
5. **Given** 用户为包含 Stale 工况的批处理准备计划，**When** 用户创建、编辑或保存计划，**Then** 计划操作可继续；**When** 用户提交新的批处理执行，**Then** 系统必须重新检查所有相关工况、完成状态、校验状态、依赖阻塞和输入版本并拒绝无效提交。
6. **Given** 计算任务已经创建或已有历史执行与结果，**When** 当前工作流后续变为 Stale，**Then** 系统不得仅因当前状态发出 Stop/Cancel 命令，既有 MockTaskService 记录必须保持不变，且不得通过现有工作流接口或任务事件追溯地使历史结果失效。
7. **Given** 工作流定义为当前步骤声明了多个适用依赖，**When** 其中两个上游要求不满足而其他要求满足，**Then** 当前步骤只产生这两个来源清晰、原因独立的 blocker，不因无关步骤产生额外 blocker。
8. **Given** 当前步骤同时存在多个 blocker，**When** 其中一个上游要求重新满足，**Then** 只移除该要求对应的 blocker，其他 blocker、下游完成事实和历史校验事实保持不变。
9. **Given** 上游资源版本变化后已重新校验通过，但下游步骤的校验印章仍指向旧版本，**When** 对应 blocker 因上游要求已满足而移除，**Then** 下游 ValidationState 仍为 Stale，直到针对当前输入重新校验成功。

---

### User Story 3 - 忽略过期的异步校验结果 (Priority: P2)

作为持续编辑配置的用户，我希望较早版本输入的校验结果即使较晚返回，也不会覆盖新版本的状态或误导我启动计算。

**Why this priority**: 防止乱序结果覆盖最新事实，是保证状态可信和计算安全的关键。

**Independent Test**: 先发起旧输入的校验，再修改输入并完成新校验，最后交付旧校验结果，验证旧结果被忽略。

**Acceptance Scenarios**:

1. **Given** 校验发起后参数身份或 revision 已更新，**When** 旧参数版本的校验结果返回，**Then** 该结果不改变当前校验状态、问题列表或可执行操作。
2. **Given** 校验发起后 scope、definitionVersion、参数身份与 revision、或实际参与校验的资源身份与 revision 任一发生变化，**When** 原校验结果返回，**Then** 该结果被视为过期并丢弃。
3. **Given** 资源 revision 已变化但新旧 fingerprint 相同，**When** 系统判定原校验是否仍然有效，**Then** 原校验仍被视为 Stale，不在本阶段跨 revision 复用。
4. **Given** 校验时某个实际参与校验的资源 fingerprint 未提供或尚未验证，**When** 当前输入与校验印章比较，**Then** 系统不得把缺失的 fingerprint 当作内容相同证据，且不得绕过身份与 revision 检查。
5. **Given** 用户只修改网格颜色、透明度或显示缩放等不参与工作流校验的展示属性，**When** 系统重新计算校验有效性，**Then** 不得仅因这些展示变化将校验标记为 Stale。
6. **Given** 校验结果的 scope、definitionVersion、参数印章以及所有实际校验资源印章与当前状态完全匹配，**When** 结果返回，**Then** 结果被接受并产生新的工作流事实版本。

---

### User Story 4 - 保持现有界面和调用方兼容 (Priority: P3)

作为现有 AeroSimWorkbench 用户，我希望状态模型升级后，仍能按当前的 Ribbon、统一工作区树和中央页面流程完成配置、定位问题和启动计算，旧调用方不需立即迁移。

**Why this priority**: 兼容性保证可以独立交付 PR 2，避免在同一阶段强制大范围 UI 改造。

**Independent Test**: 对同一组 V2 状态生成新旧两种视图，验证旧视图按冻结映射得出，现有工作流查询、运行请求、任务事件和界面定位流程仍可使用。

**Acceptance Scenarios**:

1. **Given** 步骤存在任一阻塞项，**When** 现有调用方读取旧状态，**Then** 状态映射为 Blocked。
2. **Given** 步骤无阻塞且校验为 Stale 或 Invalid，**When** 现有调用方读取旧状态，**Then** 状态映射为 NeedsValidation。
3. **Given** 步骤无阻塞、已完成且校验有效，**When** 现有调用方读取旧状态，**Then** 状态映射为 Complete。
4. **Given** 步骤不满足上述任一条件，**When** 现有调用方读取旧状态，**Then** 状态映射为 Pending。
5. **Given** 用户从工作区树或计算准备页查看一个不可执行的操作，**When** 用户选择其修复入口，**Then** 界面仍使用现有业务页面定位逻辑，不改变 Ribbon 与工作区树的联动规则。

---

### User Story 5 - 恢复工作流事实并重算派生状态 (Priority: P3)

作为工作流管理者，我希望已提交的完成和校验事实能从存储无关的快照恢复，同时过期、依赖阻塞和动作授权仍使用当前定义与当前输入重新计算，而不是盲目相信旧派生结果。

**Why this priority**: 这是工作流事实可恢复的最小语义边界，同时避免在 PR 2 中提前冻结未评审的 JSON 和文件存储方案。

**Independent Test**: 对同一份工作流事实执行快照、重建和恢复，验证已提交事实保持一致，且所有派生状态都针对恢复时的当前输入重新计算。

**Acceptance Scenarios**:

1. **Given** 快照包含完成事实、最近校验事实与印章、definitionVersion、factRevision 和时间信息，且目标 WorkflowService 的事实存储为空，**When** 系统通过存储无关的快照往返恢复，**Then** 这些已提交事实和版本保持一致。
2. **Given** 恢复时的当前输入或工作流定义与快照中的校验印章不匹配，**When** 系统生成恢复后视图，**Then** 系统保留历史校验事实，但按 FR-010 将当前校验推导为 Stale，并使用当前依赖事实重算 blocker 和动作授权。
3. **Given** 快照中不包含 Stale、blocker、动作可用性或下一步建议，**When** 系统恢复工作流，**Then** 系统能从已恢复事实与当前输入完整重算这些派生状态。
4. **Given** 目标事实存储已有高于快照的 factRevision，**When** 用户尝试恢复该旧快照，**Then** 系统返回 Conflict，保持当前 facts、draft、pending validation、revisions 和视图不变，不产生部分恢复。
5. **Given** 目标事实存储已有与快照相同 revision 且内容完全相同的 facts，**When** 重复恢复同一快照，**Then** 系统将其视为幂等成功，不改写事实版本，也不产生无意义的视图版本或业务信号；相同 revision 但内容不同必须返回 Conflict。
6. **Given** 快照已成功恢复，**When** 随后接受一次新的事实修改，**Then** 新 factRevision 必须从恢复后的最高有效 revision 继续单调递增。

### Edge Cases

- 已完成步骤的上游输入在一次校验前连续修改多次时，已提交的 Completed 始终保留，只有与最新输入印章匹配的校验结果可被接受。
- 已完成步骤在编辑草稿期间显示 InProgress，但草稿不是已提交事实；取消、校验草稿失败或保存失败均不得不可逆地改变已保存状态。
- 同一步骤有多个上游阻塞项时，所有阻塞项都保留可诊断信息，不仅显示第一个。
- 上游依赖恢复时，只移除已重新满足的声明要求所对应的 blocker，保留其他 blocker，且不改写该步骤的完成事实或历史校验事实。
- blocker 解除不等于下游校验恢复有效；如果下游校验印章与当前输入不匹配，ValidationState 仍为 Stale。
- 未被当前步骤的工作流定义声明为适用依赖的步骤，其失败或变化不得产生 blocker。
- 工作流定义判定步骤不适用时，该步骤明确表示为不适用，而不是伪造一个完成状态。
- 已完成且校验有效的步骤同时被依赖阻塞时，新视图保留 Completed 和 Valid，旧视图按优先级映射为 Blocked。
- 旧数据没有可用的校验输入印章时，不得推断为 Valid；在重新校验前按 NotValidated 处理。
- 被丢弃的异步校验结果不得递增事实版本，不得清除新版本问题，也不得发出可观察的状态倒退。
- 资源 revision 变化时，即使 fingerprint 相同，原校验仍为 Stale；本阶段不跨 revision 复用校验结果。
- 未提供或未验证的 fingerprint 不是内容相同的证据，也不能代替身份与 revision 比较。
- 纯展示属性不在工作流校验输入集中；MeshPresentation 的颜色、透明度、可见性和显示缩放变化不导致计算输入失效。
- 恢复事实快照时不相信旧的 Stale、blocker、动作可用性或下一步建议；它们必须根据恢复时的当前定义、当前输入与当前依赖事实重新计算。
- 空事实存储允许恢复合法快照；已有事实存储不得被更低 revision 的旧快照覆盖，相同 revision 只允许内容完全相同的幂等恢复，任何 Conflict 或结构校验失败都不得产生部分修改。

## Requirements *(mandatory)*

### Scope and Boundaries

**In scope**:

- 工作流步骤的完成状态、校验状态、适用性和依赖阻塞的独立表达。
- 工作流事实与由当前输入、工作流定义和依赖关系推导出的状态。
- 存储无关的工作流事实快照、恢复语义和往返验收。
- 过期异步校验结果的识别和丢弃。
- V2 视图与现有工作流视图、运行请求、任务事件及 UI 页面定位流程的兼容。
- 能够通过现有 Mock 和自动化测试重现本规格中的状态转换、阻塞和乱序结果。

**Out of scope**:

- StateConfig、BatchPlan、BatchRun、Task/Attempt、日志监控、多网格和结果查询的后续契约升级。
- 正式工程 JSON schema、文件读写、schema 迁移和存储适配器，以及真实专业校验、求解器启动、网格读取和后处理算法。
- Ribbon、统一工作区树、中央页面、布局或任务监控交互的重新设计。
- 删除现有工作流接口或要求现有调用方立即迁移。
- 已创建计算任务的继续、重试或终止规则；这些规则由任务的不可变执行快照和后续 Task/Attempt 范围决定。

### Functional Requirements

- **FR-001**: 系统 MUST 对每个适用步骤独立表达完成状态（NotStarted、InProgress、Completed）、校验状态（NotValidated、Valid、Invalid、Stale）和零个或多个依赖阻塞项。
- **FR-002**: 系统 MUST 单独表达步骤是否适用；不适用不得作为第四种完成状态，并必须可追溯到决定其适用性的工作流规则。
- **FR-003**: 新建作用域的适用步骤 MUST 初始为 NotStarted 和 NotValidated，并根据当前依赖事实推导阻塞项。
- **FR-004**: 用户直接重新编辑已完成步骤时，当前编辑会话 MUST 显示 InProgress；编辑草稿 MUST 与最近一次已提交的配置事实、版本、完成事实和校验事实区分。
- **FR-005**: 专业配置成功保存后，系统 MUST 提交新配置版本并将完成状态记录为 Completed，但 MUST NOT 因保存自动将校验状态设为 Valid；有旧校验印章且印章不匹配时 MUST 推导为 Stale，从未校验时 MUST 保持 NotValidated。
- **FR-006**: 用户取消编辑或保存失败时，系统 MUST 丢弃未提交草稿对工作流的影响，并 MUST NOT 改变最近已提交的配置事实、版本、完成事实、校验事实或校验输入印章。
- **FR-007**: 上游输入变化时，系统 MUST 保留已完成步骤的 Completed 事实，将不再匹配的校验状态推导为 Stale，并重新计算所有受影响的依赖阻塞。
- **FR-008**: 校验通过或失败 MUST NOT 改变完成状态；它们必须分别产生 Valid 或 Invalid，并保留与该校验相关的输入印章和问题。
- **FR-009**: 系统 MUST 提供存储无关的工作流事实快照与恢复语义。快照 MUST 保留完成事实、最近校验事实及其输入印章、definitionVersion、factRevision 和相关时间信息；Stale、依赖阻塞、操作是否可用及下一步建议 MUST 从恢复的事实和恢复时的当前定义、输入与依赖事实重新推导，不得作为快照中的独立权威事实。合法快照 MUST 可恢复到空事实存储并保持已提交事实与版本一致；已有事实存储的当前 factRevision 高于快照时 MUST 返回 Conflict，相同 revision 仅在事实内容完全相同时允许幂等成功，否则 MUST 返回 Conflict；任何拒绝或校验失败 MUST NOT 产生部分修改。成功恢复后的下一次事实修改 MUST 从恢复的最高有效 revision 继续递增。本功能 MUST NOT 冻结或实现正式工程 JSON schema、文件 IO、schema 迁移或存储适配器。
- **FR-010**: 每个校验结果 MUST 关联实际校验时使用的精确输入印章，包括 scope、definitionVersion、参数身份与 revision、每个实际参与校验的资源身份与 revision，以及参与校验且已验证的 fingerprint。系统 MUST 在接受异步结果或判定现有校验仍然有效前逐项与当前输入比较；任一已记录项不匹配时 MUST 丢弃异步结果或将原校验推导为 Stale。相同 fingerprint MUST NOT 覆盖 revision 差异；未提供或未验证的 fingerprint MUST NOT 被当作内容相同的证据或用于绕过身份与 revision 检查。比较 MUST 仅包含实际影响当前工作流校验的输入；MeshPresentation 等纯展示属性 MUST NOT 导致计算输入失效。本功能 MUST NOT 跨 revision 复用旧校验结果。
- **FR-011**: 被丢弃的校验结果 MUST NOT 改变事实版本、当前问题、依赖阻塞、操作可用性或任何对用户可观察的状态。
- **FR-012**: Completed + Stale MUST 只阻止创建新的计算执行及其他明确依赖“当前输入校验有效”的业务动作；页面浏览、导航、配置编辑、保存和重新校验 MUST 始终允许。对任何需要当前有效校验的动作，授权判定 MUST 同时要求相关步骤适用、CompletionState 为 Completed、ValidationState 为 Valid、无依赖阻塞，且校验输入印章与当前输入版本一致；NotValidated、Invalid、Stale 或任一其他条件不满足时 MUST 拒绝该动作。批处理计划可被创建、编辑和保存，但每次提交新批处理执行时 MUST 对所有相关工况重新执行该授权判定。
- **FR-013**: 每个工作流步骤的依赖要求 MUST 由工作流定义显式声明，并至少能声明上游完成状态、当前校验有效性和输入版本匹配要求。系统 MUST 仅对未满足的已声明适用依赖分别推导 DependencyBlocker，并 MUST NOT 因无关步骤的失败或变化阻塞当前步骤。每个 blocker MUST 标识依赖来源、未满足条件、稳定原因代码、可理解说明和可用目标页面；同一个下游步骤内 `(dependencyStepId, code)` MUST 唯一，重复定义 MUST 被工作流定义校验明确拒绝；多个依赖 MUST 分别计算，所有当前 blocker MUST 同时可见。本功能不为此新增公共 blocker ID 类型。
- **FR-014**: DependencyBlocker MUST 从当前依赖事实动态推导，不得作为可手工改写的独立权威事实。某条依赖要求重新满足时，系统 MUST 只移除该要求对应的 blocker，MUST NOT 清除其他仍有效的 blocker，也 MUST NOT 改变下游已保存的完成事实或历史校验事实。DependencyBlocker、CompletionState 和 ValidationState MUST 分别推导与表达；blocker 解除后，下游 ValidationState 仍 MUST 按 FR-010 判定，印章不匹配时保持 Stale 直到对当前输入重新校验成功。
- **FR-015**: 每次被接受的工作流事实变化 MUST 产生更新的事实版本；快照恢复是唯一保留已记录历史 factRevision 的导入操作，但不得使已有事实存储发生 revision 回退，且恢复后的下一次事实修改 MUST 继续递增。每个对外可观察的视图变化 MUST 产生可区分新旧的视图版本，幂等恢复不得制造无意义版本。
- **FR-016**: 系统 MUST 以 V2 工作流事实为唯一权威状态来源，并从同一份事实产生 V2 视图和现有视图，不得维护两套可分叉的状态。
- **FR-017**: 现有视图的映射优先级 MUST 固定为：任一阻塞项 → Blocked；否则 Stale 或 Invalid → NeedsValidation；否则 Completed 且 Valid → Complete；其余 → Pending。该映射 MUST NOT 反向写回为 V2 事实。
- **FR-018**: 现有工作流查询、运行请求、任务事件输入以及新旧视图变化通知 MUST 在兼容期内可同时使用，既有调用方不得因本功能被强制迁移。
- **FR-019**: 工作流操作的禁用原因、问题作用域和目标页面 MUST 与现有 Ribbon、统一工作区树和中央业务页面定位逻辑兼容，不得要求重建导航结构。
- **FR-020**: Mock 工作流 MUST 能模拟完成、校验、输入变化、依赖阻塞、阻塞恢复和异步结果乱序，同时 MUST 继续明确表示其不是真实专业校验或求解执行。
- **FR-021**: 自动化验收 MUST 至少覆盖全部用户故事、冻结的旧视图映射矩阵、每种校验输入印章不匹配、多阻塞项以及阻塞恢复，并必须保持现有工作流与 UI 集成验收通过。
- **FR-022**: 当前工作流视图 MUST 只用于门控新的计算提交，MUST NOT 直接改变或否定已创建计算任务所固定的输入；已创建任务、历史执行和结果 MUST 按各自的不可变执行快照及任务状态机处理。当前工作流变为 Stale 时，工作流服务 MUST NOT 因此发出 Stop/Cancel TaskCommand，MUST NOT 改写既有 MockTaskService 记录，也 MUST NOT 通过现有接口或任务事件追溯地使历史结果失效；具体 Task/Attempt 状态机仍不属于本功能。
- **FR-023**: 业务动作的最终授权结果 MUST 由 WorkflowService 或拥有该规则的相应业务服务统一判定；UI MUST 只展示授权状态、禁用原因和导航目标，并提交已授权的请求，MUST NOT 自行放宽、绕过或重新定义校验规则。

### Key Entities

- **Workflow Scope（工作流作用域）**: 一组工作流状态所属的工程与工况上下文；是校验结果能否被接受的比较条件之一。
- **Workflow Step（工作流步骤）**: 用户可完成和校验的配置单元，具有适用性、完成状态、校验状态、阻塞项、摘要和目标页面。
- **Completion Fact（完成事实）**: 表示步骤未开始、进行中或已完成的独立事实，不含校验有效性。
- **Validation Fact and Input Stamp（校验事实与输入印章）**: 记录最近一次被接受的校验成败、问题，以及实际校验时使用的 scope、definitionVersion、参数身份与 revision、资源身份与 revision 和已验证 fingerprint；不包含不影响校验的纯展示属性。
- **Editing Draft（编辑草稿）**: 尚未成功保存的当前编辑会话；可使会话视图显示 InProgress，但不是已提交配置事实，也不能改写最近已提交的完成或校验事实。
- **Dependency Blocker（依赖阻塞项）**: 工作流定义中某条适用依赖要求当前未满足时，从当前上游事实推导出的禁止原因；包含来源步骤、未满足条件、原因、说明和修复定位，不是可手工改写的独立业务事实。
- **Workflow View（工作流视图）**: 向界面和其他调用方提供的当前步骤、操作、问题与建议快照；V2 视图与现有视图由同一事实派生。
- **Action Authorization（动作授权）**: 业务服务根据动作是否要求当前有效校验，结合适用性、完成状态、校验状态、依赖阻塞和输入版本产生的可执行结果及原因；UI 不是该结果的权威来源。
- **Workflow Facts Snapshot（工作流事实快照）**: 与具体存储格式无关的可恢复事实集，包含已提交完成事实、最近校验事实与印章、definitionVersion、factRevision 和相关时间信息；不保存 Stale、blocker、动作授权或下一步建议等派生状态。默认恢复目标为空事实存储；非空存储使用 scope、revision 和事实内容执行冲突/幂等检查。

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 在全部上游输入变更验收场景中，100% 保留已提交的 Completed，且对有旧校验印章的场景 100% 显示 Stale；在直接编辑场景中，100% 仅在编辑会话显示 InProgress，取消或保存失败对已提交事实的改变次数为 0。
- **SC-002**: 对 NotValidated、Invalid、Stale、未完成、存在阻塞项或输入版本不匹配的场景，所有需要当前有效性的新执行操作 100% 被禁止；对满足全部前置条件的场景 100% 允许继续；在 Completed + Stale 场景中，浏览、导航、编辑、保存和重新校验的可用率为 100%。
- **SC-003**: 在至少 100 次可重复的乱序校验结果验收中，旧 scope、definitionVersion、参数身份或 revision、资源身份或 revision、或已验证 fingerprint 的结果覆盖新状态的次数为 0；纯展示属性变化误导致校验失效的次数为 0。
- **SC-004**: 冻结的旧视图映射矩阵中 100% 的状态组合按 Blocked → NeedsValidation → Complete → Pending 的优先级得出预期结果。
- **SC-005**: 本规格的每个验收场景都有至少一个可重复的自动化验收，且新增验收与全部现有工作流、Mock 和 UI 集成验收的通过率为 100%。
- **SC-006**: 所有被禁止的计算操作都提供至少一个可理解原因；在已知目标页面的场景中，100% 提供与现有导航规则兼容的修复定位。
- **SC-007**: 验收人员可在不查阅内部实现的情况下，仅通过状态视图和操作结果区分“已完成但过期”、“已完成且有效”与“因依赖被阻塞”三种场景，目标识别正确率为 100%。
- **SC-008**: 在声明多个依赖、局部依赖恢复、无关步骤失败和上游版本变更的验收矩阵中，100% 的 blocker 与工作流定义中当前未满足的适用依赖一一对应，且 blocker 解除误改下游完成或历史校验事实的次数为 0。
- **SC-009**: 工作流事实快照在至少 100 次空存储往返恢复中，已提交事实、输入印章、definitionVersion 和 factRevision 丢失或改写的次数为 0；恢复后使用旧派生状态而未按当前输入重算的次数为 0；旧快照覆盖更高 revision、相同 revision 不同内容、重复幂等恢复、恢复失败零部分修改及恢复后首次修改递增的验收结果 100% 符合 FR-009/FR-015。

## Assumptions

- 本规格以远端 `main@3f6e156` 中的公共契约和已冻结《公共数据契约升级设计与实施计划》为契约基线。
- 《AeroSimWorkbench 软件界面操作逻辑说明》不在当前 `main` 文件树中；本规格使用仓库已获取历史分支 `codex/main-window-layout` 中的完整版本作为 UI 操作逻辑参考，不将其误述为 `main` 已合并文档。
- 已将历史 UI 文档与 `main@3f6e156` 的实际主窗口代码、UI 模块说明和自动化验收核对：统一工作区树、Ribbon 联动、中央页面恢复、帮助页不改变业务页、WorkflowView 用于运行门控且不占用导航树等与本规格无冲突。若历史文档与当前代码存在其他差异，以当前代码、`modules/ui/README.md` 和现行自动化验收为准，不扩大 PR 2 的 UI 范围。
- 第一版继续采用一个工作区对应一个 Project，现有工程和工况作用域语义不在本功能中重新定义。
- 参数、网格、工况和任务数据仍由各自负责的业务领域管理；工作流只保留判定状态所需的轻量事实和输入印章。
- 工作流可获得校验时所需的当前定义版本、参数快照、资源版本或内容标识；如果任一权威输入无法确定，校验不能被视为当前有效。
- 本阶段的 Mock 和自动化验收用于证明状态语义、兼容映射与异步消息行为，不证明真实专业校验、工程持久化或求解正确性。
- 工作流事实快照的往返验收可使用 Mock 或内存中的存储无关表达；它不代表正式 JSON 格式、工程文件 IO 或迁移协议已获得批准。
