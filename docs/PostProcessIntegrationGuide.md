# 后处理集成准备（评审稿）

状态：待评审的集成方案，**不是**后处理功能验收。主工程在 `app/` 装配；正式 VTK 视图与空间后处理位于 `modules/visualization/`，主窗口位于 `modules/ui/`，多工况数据须与配置及工程服务对齐。

## 盘点与边界

- 已有 `References.h` 的 `ResourceRef`、`MeshRef`、`ResultRef` 和 `ScopeRef`，`DataAccess.h` 的 `IMeshAccess`/`IResultAccess`，以及 `TaskModels.h` 的任务/结果事件。继续用这些类型指代工程、文件、网格版本、任务和工况；不要在后处理再造工程或任务记录。
- `IntegrationPorts.h` 的 `IWidgetHost` 可将最终三维控件插入主窗口；`WidgetSlot::Viewport` 已定义。`ValidationResult`、`ErrorInfo` 可承载校验和错误说明。`PostProcessRefs.h` 已提供轻量 `DatasetRef`、`SceneObjectRef`；多文件/时序元信息、字段描述、场景操作、六分量/曲线数据协议仍待评审。
- `modules/visualization/` 已有 `aerosim_spatial_features`：提供多切面、二维等值线、三维等值面和 VTP 特征导出，且可独立测试。它是无界面的 features 层，不创建 renderer 或场景对象。现有 Mock 窗口的视口仍是占位控件；数据读取、统一场景入口和正式工作区仍待接入。
- 设计中的 `VTKViewer` + `VisualizationManager` 构成统一显示链路。若各功能独占 `vtkRenderer`、另存完整网格或自行控制主窗口，会造成重复、互相覆盖或版本漂移。建议接入时共用数据目录与场景入口；具体操作协议见[契约草案](PostProcessContracts.md)。

## 目标依赖方向与模块目标

```text
modules/ui/ 主窗口与后处理工作区
   -> 公共标识及展示服务 -> modules/visualization/ 数据/特征/场景服务
                                -> 格式适配器、VTK 8.2
app/ 应用装配 -> 各服务及 IWidgetHost；CLI 只依赖 Core 服务
```

模块按数据读取/规范化与数据集/帧索引、空间特征、统一场景入口、非 UI 多工况分析、后处理工作区拆分。当前已定义 `aerosim_spatial_features`；`aerosim_visualization_data`、`aerosim_visualization_scene`、`aerosim_postprocess_analysis` 是后续统一命名，尚未定义。后处理工作区归 `modules/ui/`，不在 visualization 中创建第二套主窗口。无 VTK 的数据索引、公式元信息不应强制链接 VTK；CLI 不依赖 Widgets。

## 场景及数据所有权草案

数据目录/缓存拟持有归一化网格、场数组及文件/时间索引；数据集标识与 `ResultRef`、`MeshRef` 的映射和有效性需明确。各特征功能只读输入快照并返回结果句柄/错误。场景服务拟维护 `sceneObjectId -> {datasetId, 类型, 可见状态, 样式, VTK pipeline/actor}`；Viewer 拟持有 `QVTKOpenGLNativeWidget`、`vtkGenericOpenGLRenderWindow`、`vtkRenderer`，统一增删 actor 和调度 Render。界面通过场景服务操作，不直接替换 renderer；不同特征对象不应相互清空。对象撤销、数据失效与缓存释放顺序仍需在实现评审中验证。

Qt QWidget、render window、renderer、mapper/actor 的场景修改和 `Render()` 均在 GUI 线程；昂贵的文件 IO、插值、几何提取、导出在工作线程异步执行。用 `Qt::QueuedConnection` 将轻量任务 ID/参数快照送往工作端，将只读结果句柄或可安全转移的 VTK 数据所有权送回 GUI 线程；转交之后工作端不得并发修改。Qt 自定义消息类型先注册，取消/关闭时屏蔽过期回调。VTK 8.2 的具体跨线程对象传递策略须在真实数据路径验证，不能默认 vtkActor 可跨线程操作。错误包含操作 ID、数据集 ID、错误码和说明；读取失败不创建半成品场景对象。导出使用数据/特征快照，不依赖当前显隐或色谱，除非明确是截图/动画。

## 分批接入顺序

1. 数据读取路径先以[测试夹具](PostProcessTestData.md)校对网格、字段、版本与边界，并确定数据目录的权威来源。
2. 评审 Viewer/场景服务的最小 add/update/remove/visibility 路径，再接入多文件列表与时间索引；后处理工作区通过主窗口 `IWidgetHost` 插入视口。
3. 接入多截面、等值线/面、流线、迹线和特征导出；每个功能只操作自己的 sceneObjectId。
4. 在统一渲染窗口接入时序播放与逐帧导出；数据路径补充结构转非结构和通用格式输出。
5. 对齐字段、单位与坐标系后接入表格、公式、曲线及模型能力。每一批先独立构建/自测、完成契约评审，再集成并运行全套 CTest。

每个功能接入时应包含源代码及独立 CMake 目标、接口说明、样例输入/预期输出、可复现实测、线程及所有权说明、错误路径测试和未解决问题。修改 `contracts/` 的 PR 先完成跨模块接口评审。16 项具体要求见[功能交接表](PostProcessTaskHandoff.md)。
