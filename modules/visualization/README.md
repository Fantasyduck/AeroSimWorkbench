# 可视化与后处理模块

`modules/visualization/` 是 AeroSimWorkbench 后处理的**真实数据、计算与渲染逻辑层**。它与 `modules/ui/` 中的 `PostProcessWorkbenchWindow` 共同组成一个后处理工作台，但本目录不创建窗口、不定义 Ribbon 布局，也不把 Qt Widget 作为服务接口的一部分。

后处理的输入是上游已经产生的结果选择：结果集、Zone/组件、边界或表面、物理字段、点/单元、时间帧、监控记录或多工况集合。当前上游模块尚未完成时，可以由 Mock 提供等价的轻量选择上下文；Mock 只替换输入来源和任务生命周期，不能替代真实的结果读取、VTK 算法或数值验收。

## 与后处理工作台的关系

后处理采用一个固定主框架，而非按入口维护多套窗口：

```text
上游选择 / Mock 选择
        │  DatasetRef、SceneObjectRef 等轻量标识与元信息
        ▼
应用装配与后处理服务
        ├──────────────► modules/visualization/（本目录）
        │                 读取、计算、场景、导出
        ▼
modules/ui/postprocess/PostProcessWorkbenchWindow
        ├── 顶部：创建对象、分析、工具、导出
        ├── 左侧：数据集树、显示图层、派生对象
        ├── 中央：VTK Viewer 注入位置
        ├── 右侧：随当前选择切换的上下文检查器 / Probe
        └── 底部：时间轴、任务和消息
```

UI 的职责是发起操作、展示状态和编辑轻量参数；本目录返回可显示的结果标识、元信息及不可变计算结果。UI 不直接持有 VTK `renderer`、`actor` 或大型网格数组，`analysis` 也不依赖 QWidget。

当前 UI 源码位于：

- `modules/ui/include/aerosim/ui/postprocess/`
- `modules/ui/src/postprocess/`
- `modules/ui/tests/postprocess/`

工作台的空间功能按 Tecplot 式范式组织：左侧全局显示图层和派生对象，中央 Frame/视图，右侧当前对象的局部样式、参数或 Probe 信息。它不表示现有的 VTK、读取或算法已经实现。

## 后处理功能范围

### 空间可视化与派生对象

- 结果集导入与网格、线框、表面、边界显示；
- 标量云图、色标、透明度、矢量和局部 Zone/边界样式；
- 点/单元拾取与 Probe 查询（坐标、字段值、所属 Zone、时间帧）；
- 多切面、物面/切片等值线、三维等值面；
- 表面流线、流场轨迹/迹线；
- 相机交互、适配视图和派生特征几何导出。

### 时间、曲线与分析

- 多文件及时间帧读取、帧定位、播放和时序动画导出；
- 残值、监控量与系列线曲线的显示、比较和导出；
- 多工况/多状态汇总及表格（含 Excel）输出；
- 用户公式、气动力/力矩及舵效计算；
- AI 气动力建模与舵效预测。

其中轨迹是“读取已有轨迹”还是“由非定常场计算 pathline/streakline”，以及 AI、通用格式和多工况的输入语义，必须在相应实现任务开始前确认；本 README 不将它们冻结为公共契约。

## 目录与依赖边界

```text
data/       结果文件适配、结果集、Zone/边界、字段、时间帧和单位元信息
scene/      唯一可使用 VTK renderer/mapper/actor 的场景、图层、拾取与相机适配
features/   从只读数据生成切面、等值特征、流线、迹线和可导出几何
analysis/   曲线、多状态、公式、表格、气动力/舵效与 AI；不依赖 scene 或 QWidget
tests/      每个实际目标的读取、算法和 UI 接入测试
```

建议的数据依赖是：`data → features → scene`；`analysis` 读取 `data` 与特征结果的轻量描述，但不依赖 `scene`。实际 CMake 目标只在对应子模块已有独立实现和测试后新增并接入根工程；当前目录是骨架，不应把尚未验证的草案伪装成已实现能力。

## UI 操作如何落到逻辑层

| 工作台操作 | 逻辑层职责 | 可见结果 |
| --- | --- | --- |
| 导入结果 | `data` 读取、校验并建立结果集索引 | 数据集树中出现 Zone、字段和时间帧 |
| 网格/云图/显示图层 | `scene` 创建或更新对应显示层 | 中央 Viewer 更新，右侧显示局部样式 |
| 添加切面、等值面、流线、轨迹 | `features` 校验输入并计算；`scene` 注册显示对象 | 左侧出现派生对象，中央显示结果，右侧编辑参数 |
| Probe | `scene` 拾取，`data` 按位置/单元查询字段 | 右侧显示点/单元的上下文数据 |
| 时间轴与动画 | `data` 定位帧；`scene` 更新帧；导出模块编码 | 画布切帧或生成动画文件 |
| 曲线、公式、舵效、多工况、AI | `analysis` 校验数据并异步计算 | 曲线、表格、报告或模型结果显示/导出 |

复杂功能不应一律弹出孤立窗口：创建切面、等值面、流线和轨迹时，优先在左侧建立“草稿派生对象”，右侧进入该对象的参数页，确认后写入场景；分析类任务打开配置页/向导，执行后产生曲线、表格或结果对象。文件选择、导出配置等短暂交互可使用对话框。

## 实现与验收原则

1. 每项功能明确输入类型、必需字段/单位、支持的上下文和禁用原因；按钮灰显只是可发现性，服务层必须再次校验。
2. 每项功能先在所属子目录形成最小可构建目标和独立测试，再接入 `app/` 和后处理工作台。
3. 从工作台实际触发后，应得到可见的真实结果、进度或结构化失败；仅有按钮、信号、日志或静态示意图不算端到端完成。
4. 耗时读取与计算异步执行；回传结果携带任务和数据版本，数据更新或会话关闭后不得写回过期结果。
5. 跨模块仅传轻量 ID、元信息或不可变结果，避免将大型数组或 VTK 对象放入 `contracts/`。

### 已实现目标与其依赖

| 目标 | 依赖 | 说明 |
| --- | --- | --- |
| `aerosim_monitor_data` | Qt5::Core | 监控日志读取、实时源与累加器，无界面即可使用 |
| `aerosim_monitor_analysis` | Qt5::Core + Eigen 3 | 序列模型与曲线拟合（最小二乘用 Eigen 列主元 QR） |
| `aerosim_monitor_charts` | Qwt 7.5 | 唯一链接绘图库的目标；换库只需重写它 |

Eigen 为纯头文件库，不需要链接，只需包含路径。查找逻辑在 `analysis/CMakeLists.txt`，两种布局都接受（`<eigen/Dense>` 与 `<Eigen/Dense>`）：先试 `find_package(Eigen3)`，否则用 `-DAEROSIM_EIGEN_ROOT=<包含 eigen/Dense 的目录>`。可以不另装 Eigen，直接用 VTK 自带的那份（`ThirdParty/eigen/vtkeigen`）。Qwt 的查找同理，见 `charts/CMakeLists.txt` 与其中的 `AEROSIM_QWT_ROOT`。两个变量都不写默认绝对路径。

用 CMake 构建并测试本目录（含 Qwt 与 Eigen 的完整环境变量设置）：

启用 `AEROSIM_BUILD_VISUALIZATION` 后，使用仓库的构建脚本或 CMake 构建，再用 CTest 运行监控数据和图表测试。

构建环境、正式 VTK 版本、Windows 验证与部署方式见根目录 `AGENTS.md`、`docs/DevelopmentEnvironment.md` 和 `docs/windows-deployment.md`；后处理接口和接入约定见 `docs/PostProcessIntegrationGuide.md`、`docs/PostProcessContracts.md` 与 `docs/PostProcessTaskHandoff.md`。若这些草案与真实代码或更新的实测记录冲突，应先核对再实现。
