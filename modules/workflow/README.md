# 工作流管理

本模块包含 `IWorkflowService` 实现、步骤依赖与统一动作校验、WorkflowViewV2 和恢复。通过数据、配置、运行模块的公共接口读取事实并协调任务请求；不依赖 QWidget，不实现专业算法或求解器调度。工作流视图以 V2 facts 为唯一权威源，不再保留 legacy view 或单向 adapter。
