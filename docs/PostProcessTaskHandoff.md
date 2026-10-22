# 五人专项任务交接表（16 项，待成员确认）

本表的专项 A～E **不是**主工程七人 A～G。建议目标名只是评审占位；目前 `modules/visualization/` 没有正式 C++ 实现，不能声称这些目标已构建。总体 C 与专项组协调 VTK/数据服务，总体 B 与专项 B 协调主窗口，总体 D/G 审核工况/工程数据，总体 A 审核公共契约和构建装配。使用既有 `ResourceRef`/`MeshRef`/`ResultRef` 和新增标识，不重新定义任务/工程事实。下表“场景”均指向同一 Viewer/registry，渲染操作必须在 GUI 线程。

| # / 专项成员 | 输入 | 输出/接口交接 | 依赖 | 建议接入位置与夹具 |
| --- | --- | --- | --- | --- |
| ① A 网格导入 | MSH/CAS/CGNS 文件资源、格式版本、边界元数据 | 统一网格/Zone/边界描述与 `MeshRef`，结构化错误；读取器接口 | 既有资源索引；格式库由 A 报告 | `visualization` 的 data/reader 目标；先用 `mesh.json` 与 `cube_t00.vtk` 校对几何，不把它们当 CAS/CGNS。 |
| ② A 结构转非结构 | 原始 CGNS 结构化 Zone、边界映射 | 非结构单元拓扑、原节点/边界对应表与网格版本 | ①、CGNS 约定 | data/converter 目标；规则立方体预期 8 hex、6 边界组；真实 CGNS 另供。 |
| ③ E Excel 汇总 | 按 `caseId` 的参数与六分量、单位/坐标系 | 结构化表和 Excel 文件或明确失败；导出端口 | 工况数据来源经总体 D/G 确认 | analysis/export 目标；`cases.csv` 四工况，Excel 后端和格式待定。 |
| ④ C 多截面 | datasetId/meshVersion、平面参数和对象 ID | 多个独立特征句柄及 SceneObjectRef；更新/显隐/删除 | ①、场景入口 | features/slice + scene；立方体 x=0，平面取 9 个网格节点。 |
| ⑤ C 物面/切片等值线 | 面/切片句柄、字段 key、等值和关联方式 | 线几何句柄、来源及错误 | ④、①、字段规范 | features/contour + scene；`Phi=x` 在 x=0 截面的退化情形另定处理，推荐 z=0 面上的 Pressure=100000。 |
| ⑥ C 三维等值面 | datasetId、标量字段、等值/缺失值策略 | 面几何句柄、sceneObjectId | ①、字段规范 | features/iso + scene；`Phi=0` 为 x=0 平面。 |
| ⑦ D 表面流线 | 物面、点/单元速度场、种子、积分参数 | 曲线特征句柄、停止原因及 sceneObjectId | ①、字段规范、场景入口 | features/streamline + scene；`Velocity=(1,0,0)`，路径 x 单调增。物面投影规则待定。 |
| ⑧ D 迹线 | 已有轨迹文件**或**非定常帧序列、粒子种子 | 轨迹数据句柄/时刻、sceneObjectId | ①、⑨/⑩（若计算非定常） | features/trajectory + scene；读取/计算范围先确认，夹具只覆盖恒定速度和三帧。 |
| ⑨ B 多文件展示 | 一组 `ResourceRef`/`ResultRef`、加载请求 | fileId/datasetId 索引、列表状态、显隐对应 sceneObjectId | ①、场景入口 | data/catalog + ui；`index.json` 提供同一数据集三个文件；不同 dataset 并列的行为待确认。 |
| ⑩ B 时序动画 | frameId/物理时间/有效性、显示数据集 | 播放状态、帧切换事件与时间定位 | ⑨、场景入口、字段/网格匹配 | data/timeline + ui/scene；`cube_t00..02.vtk`，0/1/2 s。 |
| ⑪ E 自定义公式与舵效 | caseId、变量/单位、公式表达式及基准 | 结果字段、来源/校验错误、保存请求 | ③ 的表模型、总体 D/G 数据权威 | analysis/formula；`cases.csv` 的 delta=0/10 配对；语言和舵效定义待定。 |
| ⑫ E 系列线显示输出 | 一组 caseId、x/y 字段/单位/排序 | seriesId、曲线数据及图表/CSV 等输出 | ③/⑪，曲线组件与 UI | analysis/series + ui；`curves.csv` 示例 Cz(α)，同 α 不同舵偏角保留不同 caseId。 |
| ⑬ E AI 气动力建模/舵效预测 | 工况训练表、特征/标签、划分与模型配置 | 预测表、模型文件、指标与来源/版本 | ③/⑪/⑫；模型后端和许可待确认 | analysis/ai 独立可选目标；四行样例仅用于接口/IO 测试，**不能**检验模型精度。 |
| ⑭ C 特征几何导出 | ⑤/⑥/⑦的不可变几何句柄、格式请求 | 导出文件/错误及原数据版本 | ⑤、⑥、⑦、格式库 | features/export；fixture 可检查坐标包围盒；具体格式/精度待确认。 |
| ⑮ A 通用格式输出 | 网格/Zone/边界/字段/帧与目标格式 | Tecplot/CGNS/Plot3D 合法文件和结构化错误 | ①/②、字段/时间协议、对应 SDK | data/export；**不生成伪装的后缀文件**，真实读回测试由 A 提供。 |
| ⑯ B 时序动画导出 | ⑩的帧序列、场景/相机快照与编码配置 | 逐帧图像或动画文件、导出进度/错误 | ⑩、唯一 Viewer、编解码器 | ui/animation-export + scene；三帧顺序和尺寸可测，视频编码与离屏方案待定。 |

## 数据/功能依赖总览

```text
① → ②, ④, ⑤, ⑥, ⑦, ⑨, ⑮
④ → ⑤；⑤/⑥/⑦ → ⑭；⑨ → ⑩ → ⑯，⑩ 可供 ⑧ 非定常路径
工况数据 → ③ → ⑪/⑫ → ⑬；⑪ 可提供 ⑫/⑬ 的衍生变量
全部空间特征 → 单一场景入口 → 专项 B/总体 B 的窗口
```

各成员交付时须写明输入/输出的确切头文件和目标、字段与单位、版本/失效策略、异常路径、线程归属、独立构建/测试结果、夹具实际输出，以及需要 A 协调的公共契约变化。专项 A 先交读取器最小样例；专项 B 先交窗口插入/场景调用的最小切口；专项 C/D 先交一项真实特征和场景句柄；专项 E 先交无 UI 的表格/曲线数据通路。相关成员评审通过后，才把草案写入公共接口。
