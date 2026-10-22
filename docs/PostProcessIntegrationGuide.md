# 后处理集成准备（评审稿）

状态：2026-10-22；这是架构师 A 的接收准备，**不是**五名专项成员的功能验收。专项 A～E 是后处理工作组，与主工程七人分工的 A～G 不对应。主工程仍由总体 A 装配；正式 `modules/visualization/` 由总体 C 对接，窗口由总体 B 对接，多工况领域数据还需总体 D/G 确认。

## 盘点与边界

- 已有 `References.h` 的 `ResourceRef`、`MeshRef`、`ResultRef` 和 `ScopeRef`，`DataAccess.h` 的 `IMeshAccess`/`IResultAccess`，以及 `TaskModels.h` 的任务/结果事件。继续用这些类型指代工程、文件、网格版本、任务和工况；不要在后处理再造工程或任务记录。
- `IntegrationPorts.h` 的 `IWidgetHost` 可以将最终三维控件插入总体 B 的窗口；`WidgetSlot::Viewport` 已定义。`ValidationResult`、`ErrorInfo` 可承载校验和错误说明。尚无可共享的多文件/时序身份、字段描述、场景对象协议、六分量/曲线数据协议。
- `modules/visualization/` 目前只有 README，没有生产 C++ VTK 源码或构建目标。`demos/postprocess/` 是独立 Python/PySide2/VTK 9.3 合成场前端，只验证交互构想，不是 VTK 8.2 的正式数据读取器、统一场景或算法。保留其独立进程入口和现有 Mock。
- 设计文档既有 `VTKViewer` + `VisualizationManager` 单一显示链路。若每个专项功能独占一个 `vtkRenderer`、另存完整网格或自行控制主窗口，会造成重复、互相覆盖或版本漂移。建议接入时共用数据目录与场景入口；详见 [契约草案](PostProcessContracts.md)。

## 目标依赖方向与建议目标（名称待总体 C/B 确认）

```text
总体 B 的 UI / 专项 B 窗口
   -> 公共标识及展示服务 -> 总体 C 的数据/特征/场景服务
                                -> 格式适配器、VTK 8.2
总体 A 的 app 装配 -> 各服务及 IWidgetHost；CLI 只依赖 Core 服务
```

`modules/visualization/` 内建议独立目标：`aerosim_postprocess_data`（专项 A 的读取/规范化，含专项 B 的数据集/帧索引）、`aerosim_postprocess_features`（专项 C/D 的专业算法，可分更细库）、`aerosim_postprocess_scene`（总体 C 的单一 Viewer/场景入口）、`aerosim_postprocess_analysis`（专项 E 的非 UI 分析，可能落总体 D 的配置/分析目标）、`aerosim_postprocess_ui`（专项 B 与总体 B 协调窗口）。**这些名称是接入建议，当前无对应目标，也不是已冻结的模块归属。**每个依赖可单独关闭；A 收到 PR 后再修改顶层 CMake。无 VTK 的数据索引、公式元信息不应强制链接 VTK；CLI 不依赖 Widgets。

## 场景及数据所有权草案

数据目录/缓存持有归一化网格、场数组及文件/时间索引；数据集标识指向 `ResultRef` 和 `MeshRef`，有效性独立报告。专项算法只读输入快照，在自己的命名空间创建特征结果，返回结果句柄/错误。唯一的场景服务维护 `sceneObjectId -> {datasetId, 类型, 可见状态, 样式, VTK pipeline/actor}`；Viewer 持有 `QVTKOpenGLNativeWidget`、`vtkGenericOpenGLRenderWindow`、`vtkRenderer`，统一增删 actor 和调度 Render。界面只调用场景服务，不直接持有或替换 renderer；各专项不得清空其他对象。对象撤销要先卸载 actor，再释放 pipeline 和缓存引用；失效时标记原数据版本并通知 UI，不悬挂引用。

Qt QWidget、render window、renderer、mapper/actor 的场景修改和 `Render()` 均在 GUI 线程；昂贵的文件 IO、插值、几何提取、导出在工作线程异步执行。用 `Qt::QueuedConnection` 将轻量任务 ID/参数快照送往工作端，将只读结果句柄或可安全转移的 VTK 数据所有权送回 GUI 线程；转交之后工作端不得并发修改。Qt 自定义消息类型先注册，取消/关闭时屏蔽过期回调。VTK 8.2 的具体跨线程对象传递策略须在真实数据路径上由总体 C 与专项 A/C/D 验证，不能默认 vtkActor 可跨线程操作。错误包含操作 ID、数据集 ID、错误码和说明；读取失败不创建半成品场景对象。导出使用数据/特征快照，不依赖当前显隐或色谱，除非明确是截图/动画。

## 分批接入顺序

1. 专项 A 提交一个小网格/字段读取路径，能读 [测试夹具](PostProcessTestData.md)，明确版本、边界和字段归属；总体 C 确认数据目录权威来源。
2. 总体 C 与专项 B 确认唯一 Viewer/场景服务的最小 add/update/remove/visibility 路径，再接入多文件列表与时间索引；与总体 B 的 `IWidgetHost` 协调窗口插入。
3. 专项 C 接多截面、等值线/面及特征导出；专项 D 接表面流线、迹线，每个功能只操作自己的 sceneObjectId。
4. 专项 B 接时序播放和逐帧导出（统一渲染窗口）；专项 A 接结构转非结构和格式输出。
5. 专项 E 接表格/公式/曲线，在字段/单位/坐标系规则确认后集成 AI 模型。每一批先独立构建/自测、契约评审，再由总体 A 集成、运行全套 CTest。

成员应交付源代码及独立 CMake 目标、接口说明、样例输入/预期输出、可复现实测、线程及所有权说明、错误路径测试和未解决问题。任何修改 `contracts/` 的 PR 先让涉及的成员和总体 A 评审。16 项具体交接见 [任务表](PostProcessTaskHandoff.md)。
