# 后处理公共契约（已存在／安全补充／待确认草案）

本页是接收五名专项成员实现时的讨论基线，不单方面冻结数据布局或类名。三层架构与七人总体分工继续按 `docs/phase1-integration.md` 执行。

## 已存在且可直接复用

| 现有代码 | 后处理用途 | 限制 |
| --- | --- | --- |
| `References.h`: `ResourceRef`, `MeshRef`, `ResultRef`, `ScopeRef` | 文件路径/修订、网格版本、结果所属任务和 case | 没有多个加载实例、帧或场景身份；`resourceId` 不等同加载实例 ID。 |
| `DataAccess.h`: `IMeshAccess`, `IResultAccess` | 工程步骤对网格/结果的只读有效性检查 | 不提供节点/拓扑/字段读取，不能冒充完整结果仓库。 |
| `WorkflowModels.h`: `ValidationResult`；`Telemetry.h`: `ErrorInfo` | 数据版本校验和失败报告 | 算法特有错误码尚未约定。 |
| `TaskModels.h`: `TaskEvent::ResultGenerated` | 求解结果创建通知 | 结果通知不是读取完成/场景上屏事件。 |
| `ui/IntegrationPorts.h`: `IWidgetHost`, `WidgetSlot::Viewport` | 插入最终 Viewer 控件 | 当前 Mock 的 viewport 是占位符。 |

已验证范围：现有 A 骨架 Qt 构建与 Mock 通信；**未验证**成员的真实数据或算法接口。

## 已加入、可安全使用的最小代码

`PostProcessRefs.h` 提供 `DatasetRef {datasetId, fileId}` 与 `SceneObjectRef {sceneObjectId, datasetId}`；它们只持有标识，无 VTK、路径、生命周期或业务策略。`RegisterMetaTypes.cpp` 注册两个 Qt 元类型，便于以后按值排队传递。现有契约未改字段或虚函数；是否把额外字段并入这些结构须经过五方评审。标识建议在一次工程会话内唯一，跨重载的持久化语义仍待确认。`DatasetRef` 不替换 `ResultRef`；一个原始资源可以被多次加载为不同数据集。

## 数据描述草案（未实现、待五方确认）

| 概念 | 建议内容与不变量 | 需要确认 |
| --- | --- | --- |
| 网格 | meshVersion/坐标系与单位；节点 ID + 三维坐标；单元 ID + 类型 + 有序节点 ID；Zone ID 与单元集合；边界 ID + 面片/所属 Zone | CGNS 多 Zone、混合维度/高阶单元、索引从零或源文件编号、边界法向、非连续 ID、结构化原始拓扑是否保留。 |
| 物理场 | 稳定变量 key、显示名、单位、分量数/次序、标量/矢量、点或单元关联、所属 meshVersion、缺失值掩码 | 同名不同单位、单位转换、cell↔point 插值由谁负责、张量和面数据。 |
| 数据集/文件 | datasetId（一次加载实例）、fileId（源文件索引），映射到既有 `ResourceRef`/`ResultRef`/`MeshRef`；状态与失败原因 | ID 生成/持久化、一个文件多 Zone、多文件同一时刻、路径去重、结果与网格版本匹配策略。 |
| 帧 | datasetId、frameId、稳定顺序索引、物理时间与单位、source fileId、是否完整/有效和错误 | 非均匀时间、重复/缺帧、时钟对齐、增量加载及重试。 |
| 场景对象 | sceneObjectId、datasetId、类型（原网格、切面、线、面、流线、迹线等）、来源参数版本、可见性、样式版本 | 跨帧对象复用还是重建、样式持久化、父子对象、拾取 ID、二维曲线是否属于三维场景。 |
| 多工况 | caseId 关联既有 `ScopeRef`/`ResultRef`，工况参数 key/值/单位，Fx/Fy/Fz/Mx/My/Mz 的坐标系、参考点/长度/面积/动压及单位 | 正方向、力矩基点、升阻/舵效定义、缺测值、工况排序及来源追溯。 |
| 公式/曲线 | 公式变量绑定到 `(caseId, field key, unit, association)`；曲线按 seriesId 保存 x/y 名称、单位、值和源 caseId/时刻 | 表达式语言、维度检查、跨 Zone 聚合、系数定义和预测值溯源。 |

`contracts/` 只放可跨模块按值传递的标识与元信息，**不放** `vtkDataObject`、`vtkRenderer`、节点大数组或数据缓存。具体 `MeshData`/`FieldData` 格式先由专项 A 提供候选并与总体 C/专项 C/D 联审；避免和既有 `MeshRef`/`ResultRef` 并行维护两套事实。

## 场景操作接口草案（伪接口，未加入代码）

```text
submitObject(ownerTaskId, datasetId, featureKind, immutableFeatureHandle,
             initialStyle) -> Result<SceneObjectRef, PostProcessError>
updateObject(sceneObjectId, styleOrParameters, expectedRevision)
setVisible(sceneObjectId, visible)
removeObject(sceneObjectId)
lookupObject(sceneObjectId) -> metadata only
```

场景服务持有 registry、pipeline/actors，唯一 Viewer 持有 render window/renderer；调用均投递 GUI 线程。特征服务只生成**不可变结果**并提交；修改自己的对象使用返回的 ID，不能 `RemoveAllViewProps()`。数据集卸载时先通知关联特征服务取消任务并使对象失效，再移除相关 sceneObjectId；仍在工作的结果用任务代次/数据修订号过滤。`Result<T,Error>`、style 字段、同步还是异步返回、预览对象是否保留，这些均为方案草案；专项 B/C/D 与总体 C 共同确定后才落正式头文件。耗时生成返回异步完成事件，不在 GUI 线程计算。导出可取得不可变特征数据；vtk 数据指针不得经 Qt 信号裸传和并发写。

## 接口评审清单

五名成员各自反馈：当前实现使用的数据类及 target、最小头文件/API、字段单位和坐标约定、内存/线程所有权、是否可对接 fixture、错误语义、与上述草案冲突之处。尤其确认：①轨迹是读入已有数据还是非定常计算；②是否需要单元/面数据与多 Zone；③帧与多文件组合规则；④唯一 Renderer 与 Scene registry 的负责方；⑤格式版本/Excel 后端/AI 模型文件及许可。确认前不修改他们未合并的代码。
