# 表面流线与迹线 实施方案（⑦ / ⑧）

> 状态：**待评审草案**。本文只描述实施方案与决策点，不代表接口已冻结。
> 对应交接表条目：`docs/PostProcessTaskHandoff.md` 第 20 行（⑦ 表面流线）、第 21 行（⑧ 迹线显示）。
> 编写依据：当前 `main`（ae29d72）与未合并 PR #1 的实现，以及本机 VTK 语义实测（见第 3 节）。

## 1. 范围

### 1.1 本方案要交付的

| 编号 | 功能 | 本方案产出 |
| --- | --- | --- |
| ⑦ | 表面流线 | 在指定 2D 物面/切片上积分速度场，产出曲线几何 + 每条线的终止原因 + 筛选诊断 |
| ⑧ | 迹线显示 | 定义轨迹数据的组织形态与来源适配（读取已有轨迹 / 非定常帧计算），产出可显示、可导出的轨迹几何 |

### 1.2 本方案明确不做

- **不做 UI 交互面板**。种子拾取、参数滑块属 `modules/ui/`，依赖尚未实现的 `scene/` 层（见 4.4）。
- **不新建独立渲染链路**。按 `docs/PostProcessIntegrationGuide.md` 第 10 行，禁止功能独占 `vtkRenderer`。
- **不修改 PR #1 已有实现**。⑦⑧ 在其接口上增量扩展（见 2.2）。
- **不引入新第三方库**。只允许 C++14、Qt 5.12.12、VTK 8.2.0。

## 2. 仓库现状核对（决定方案形态的事实）

### 2.1 可用基础

- 契约层已有轻量标识：`contracts/include/aerosim/contracts/PostProcessRefs.h` 的 `DatasetRef{datasetId,fileId}`、`SceneObjectRef{sceneObjectId,datasetId}`，且已由 `contracts/src/RegisterMetaTypes.cpp` 注册为 Qt 元类型。⑦⑧ 的句柄直接复用这两个类型，**不新增契约**。
- 线程与所有权规则已在 `docs/PostProcessContracts.md` 第 33 行定死：`contracts/` 不放 `vtkDataObject`、`vtkRenderer` 或节点大数组。

### 2.2 PR #1 提供、可直接复用的接口

路径：`modules/visualization/include/aerosim/visualization/features/`

- `FeatureStatus` / `FeatureError`：现成的错误码体系，⑦⑧ 只做**追加**，不改语义。
- `FeatureGeometry`：私有深拷贝持有 `vtkPolyData`，`copyData()` 返回独立副本。⑦ 的流线、⑧ 的轨迹都是 `vtkPolyData`，**可直接复用该类型作为返回载体**。
- `FeatureResult{status, std::shared_ptr<const FeatureGeometry> geometry}`。
- `FeatureProcessor::snapshot(vtkPolyData*)`：注释明确 Accept externally generated lines/surfaces (e.g. streamlines) for export —— 即 ⑦ 的输出接入 ⑭ 的通道**已经预留**。
- `FeatureExporter::writeVtp()`：QSaveFile 原子提交、拒绝默认覆盖，⑦⑧ 的导出无需自写。
- `FeatureJob`：后台提交/取消适配器，⑦⑧ 的耗时积分复用同一套取消与结果所有权语义。

### 2.3 关键缺口

1. **`scene/` 层未实现**（PR #1 的 README 明确：Viewer、对象显隐/删除、工程数据访问及完整界面尚未接入）。因此 ⑦⑧ 本次只能交付到 `features/`，场景接入是后续阶段。
2. **`FeatureOperation` 枚举只有 `Slice/ContourLines/Isosurface`**，需追加 `SurfaceStreamline` / `Trajectory` 两个值才能走 `FeatureJob`。
3. **物面来源缺失**：夹具 `cube_t00.vtk` 是 `STRUCTURED_POINTS 3x3x3`，**文件本身不编码任何边界 ID**；六个边界只存在于 `tests/fixtures/postprocess/mesh.json` 的 `boundaries` 段（6 个具名面，各含 `boundaryId` 与 quad 的 `faceNodeIds`，每面 4 个 quad / 9 个节点）。
4. **VTK 8.2.0 原为缺失**，已于 P0 阶段编译补齐（见 7.1）。本方案第 3 节的探针结论仍产生于 VTK 9.5.2，但已在 VTK 8.2.0 Debug 上用 PR #1 的 9 组既有测试间接验证了该构建可用。

## 3. 实测结论（本机 VTK 9.5.2 语义探针，脚本在 `.tools\probe_*.py`）

这些结论直接决定实现方式，不是推测：

### 3.1 表面投影：VTK 自带，无需自研

做法：造 5x5 点、16 个 `vtkQuad`（`GetCellDimension()==2`）的 z=0 平面，速度场**故意**设为 v=(1,0,5)（法向分量是切向的 5 倍），种子 (0,2,0)，RK2 前向，`MaximumPropagation=10`：

| `SetSurfaceStreamlines` | 输出点数 | 轨迹 | 结论 |
| --- | --- | --- | --- |
| `true` | 6 | z 恒为 0，x = 0 / 0.707 / 1.414 / 2.121 / 2.828 / 3.536 | 速度被**投影到切平面并归一化**后积分 |
| `false` | 8 | z 涨到 4.854，x 只到 0.971 | 沿真实 3D 方向跑出平面 |

**结论**：物面投影由 `vtkStreamTracer` 内部完成，我们只需保证输入是**全 2D 单元**的曲面。这与 PR #1 中 `contourLines` 的校验（`validate(ds, 2)` 逐单元核 `GetCellDimension()==2`）完全一致，可直接复用同一校验思路。

### 3.2 终止原因：VTK 直接写 cell array

`vtkStreamTracer` 会把 `ReasonForTermination` 写成 **cell 数组**，取值即 `enum ReasonForTermination`：`OUT_OF_DOMAIN=1`、`NOT_INITIALIZED=2`、`UNEXPECTED_VALUE=3`、`OUT_OF_LENGTH=4`、`OUT_OF_STEPS=5`、`STAGNATION=6`。

**夹具实测**：用 `mesh.json` 的 `y_plus` 面（法向 +y）+ 恒定速度 (1,0,0)，从该面首节点出发：

- 输出 3 点 1 条线，x 从 -1 单调增到 0.414，**x 单调递增成立**（正好对上 `PostProcessTestData.md` 第 15 行的断言要求）；
- `ReasonForTermination` 存在，值 = 1（OUT_OF_DOMAIN），因为 3x3x3 网格在 x 方向只覆盖 [-1,1]，一步后就出域。

**副产品**：从 `x_plus` 面（法向 +x、与速度平行）出发输出 1 点 0 条线 —— **法向与速度平行的面产生退化线**，这正是自动筛选要处理的第一类情况。

### 3.3 非定常迹线：问题所在

`vtkParticleTracer` + 自建时间管线（`TIME_STEPS`/`WHOLE_EXTENT` 均正确设置，种子为 `vtkProgrammableSource`）在夹具三帧上**只输出 1 个点、0 条线**，且该点落在 (1,0,0) 而非种子点 (-1,0,0)；把时间采样加密到 21 帧、41 帧后结果仍是 1 点或 0 线。

对照实验：用同样的三帧数据手写**跨帧插值 + 定步长积分**，得到 41 个点、x 从 -1 单调增到 1（在域边界 x=1 停住）。

**结论**：

- 夹具速度场三帧恒为 (1,0,0)，**没有真实的非定常内容**，`vtkParticleTracer` 在这种退化输入上不可靠；
- 若要交付计算迹线，**必须先解决帧数据的时间插值**（`vtkTemporalInterpolatedVelocityField`），并且**需要能真正验证的非定常夹具**，否则交付物无法验收；
- 这印证了 `docs/PostProcessContracts.md` 第 52 行把 轨迹是读入已有数据还是非定常计算 列为**待确认项**是正确的。

## 4. 接口设计

### 4.1 ⑦ 表面流线

新增 `modules/visualization/include/aerosim/visualization/features/StreamlineProcessor.h`：

```cpp
namespace aerosim { namespace visualization {

enum class StreamlineTermination {            // 与 vtkStreamTracer::ReasonForTermination 一一对应
  OutOfDomain = 1, NotInitialized = 2, UnexpectedValue = 3,
  OutOfLength = 4, OutOfSteps = 5, Stagnation = 6, Unknown = -1
};

enum class SeedMode { ExplicitPoints, Line, SurfacePoints };

struct StreamlineSeeds {
  SeedMode mode = SeedMode::ExplicitPoints;
  std::vector<std::array<double,3>> points;   // ExplicitPoints
  std::array<double,3> lineStart{{0,0,0}};    // Line
  std::array<double,3> lineEnd{{0,0,0}};      // Line
  int lineResolution = 20;                    // Line
  int surfaceStride = 1;                      // SurfacePoints：按点序号抽稀
};

struct StreamlineParameters {
  std::string velocityField = "Velocity";     // 显式字段名，禁止隐式取第一个向量
  StreamlineSeeds seeds;
  bool surfaceStreamlines = true;             // 关掉就是自由空间流线
  int integrationDirection = 0;               // 0=forward 1=backward 2=both
  int integrator = 2;                         // 0=RK2 1=RK4 2=RK45
  double maximumPropagation = 1.0e6;
  double initialStep = 0.1;                   // <=0 表示交给 VTK 按单元长度自适应
  double minimumStep = 0.01;
  double maximumStep = 1.0;
  int stepUnit = 2;                           // 1=LENGTH_UNIT 2=CELL_LENGTH_UNIT
  long long maximumSteps = 2000;
  double terminalSpeed = 1e-12;
};

struct RejectedLine {                          // 自动筛选的诊断输出
  enum class Reason { zeroLength, belowMinLength, seedOutsideDomain,
                      parallelToSurfaceNormal, duplicate, overLineLimit } reason;
  int seedIndex = -1;
  double arcLength = 0.0;
  std::string detail;
};

struct StreamlineResult {
  FeatureStatus status;
  std::shared_ptr<const FeatureGeometry> geometry;   // 复用 PR #1 类型
  std::vector<StreamlineTermination> terminations;   // 与几何中的线一一对应，顺序稳定
  std::vector<RejectedLine> rejected;                // 被筛选掉的线及原因
  std::size_t acceptedCount() const;
};

struct StreamlineFilter {
  double minimumArcLength = 0.0;      // 0 = 不过滤
  std::size_t maximumLines = 0;       // 0 = 不限制；超出时按弧长降序保留
  bool dropParallelToNormal = true;   // 剔除种子处速度与法向平行的退化线
  bool deduplicateOverlapping = true; // 起点距离 + 方向夹角双阈值
  double dedupDistanceTolerance = 1e-6;
  double dedupAngleToleranceDeg = 1.0;
};

class StreamlineProcessor {
public:
  static StreamlineResult surfaceStreamlines(vtkDataSet *surface, const StreamlineParameters &p,
                                             const StreamlineFilter &f = {});
  static StreamlineResult volumeStreamlines(vtkDataSet *volume, const StreamlineParameters &p,
                                            const StreamlineFilter &f = {});
};

const char *streamlineTerminationName(StreamlineTermination t);
}}
```

要点：

- 输入是**调用方明确选中的 2D 曲面**（物面或切片结果），与 PR #1 `contourLines` 约定一致；`surfaceStreamlines` 内部先做 `GetCellDimension()==2` 全量校验，失败返回 `UnsupportedGeometry` 并提示请传入 2D 物面或切片结果。
- 输出走 `FeatureGeometry`，**天然接入 ⑭**：`FeatureProcessor::snapshot(geometry.copyData())` 后 `FeatureExporter::writeVtp`。
- 终止原因**不塞进点数据**，而是平行数组随结果返回，避免污染导出文件。

### 4.2 ⑦ 自动筛选规则（用户来文明确要求 具体筛选规则需约定，此处提出具体规则）

按以下顺序执行，每条被剔除的线记入 `rejected` 并附原因，**不做静默丢弃**：

1. **零长度**：点数 < 2 或弧长 == 0 → `zeroLength`（对应 3.2 中 `x_plus` 面的实测现象）。
2. **最短弧长**：弧长 < `minimumArcLength` → `belowMinLength`。默认 0 即关闭，由调用方按物理尺度设定，**不设魔数默认值**。
3. **种子在域外**：`vtkStreamTracer` 返回 `NOT_INITIALIZED` → `seedOutsideDomain`。
4. **与法向平行**：种子处速度在切平面上的投影 < 1e-9 → `parallelToSurfaceNormal`（同样来自 3.2 实测）。
5. **重合去重**：起点距离 < `dedupDistanceTolerance` 且方向夹角 < `dedupAngleToleranceDeg` → `duplicate`。
6. **数量上限**：保留数超过 `maximumLines` 时按弧长降序截断，其余标记 `overLineLimit`。

这 6 条是**建议值**，需要评审确认；尤其 2 的默认值与 6 的排序键（弧长 vs 种子序号）会影响用户预期。

### 4.3 ⑧ 迹线

核心设计：**先把 轨迹数据 与 轨迹来源 解耦**，这样 读取 还是 计算 的决策不会返工显示侧。

`modules/visualization/include/aerosim/visualization/features/TrajectorySet.h`：

```cpp
struct TrajectorySet {                       // 统一内存表示，与来源无关
  FeatureStatus status;
  std::shared_ptr<const FeatureGeometry> geometry;  // vtkPolyData，线 + 点
  // 必需点数组（缺失即 InvalidInput）："Time"(1), "ParticleId"(1)
  // 可选点数组："ParticleAge"(1), "SeedId"(1)
  // 必需 cell 数组："TerminationReason"(1)，取值同 StreamlineTermination
  std::string timeUnit = "s";
  std::string coordinateFrame;              // 与 index.json 的 body-xyz 对齐
  double startTime = 0.0;
  double endTime = 0.0;
  std::size_t trajectoryCount() const;
};
```

两个来源适配器（同一 `TrajectorySet` 出口）：

- **P1 读取**：`TrajectoryReader::readVtpTrajectories(const QString &path)` / `readLegacyTrajectories(...)`，同时接受 `vtkPolyData` 内存输入。**轨迹文件格式当前仓库无任何夹具**，格式需评审确定。
- **P2 计算**：`TrajectoryProcessor::integrate(const std::vector<vtkDataSet*> &frames, const double *times, ...)`，内部用 `vtkTemporalInterpolatedVelocityField` + `vtkParticleTracer`；**在 3.3 的退化输入问题解决前不纳入 P1 交付**。

### 4.4 场景接入（后续阶段，非本次）

按 `docs/PostProcessContracts.md` 第 39-46 行，特征层只产出**不可变结果**，由场景服务 `submitObject(ownerTaskId, datasetId, featureKind, handle, initialStyle) -> Result<SceneObjectRef, PostProcessError>` 接管。⑦⑧ 各自只操作自己的 `sceneObjectId`。**该契约尚未实现，本次不落地**。

## 5. 分层与文件清单

严格遵循 `modules/visualization/AGENTS.md`：`data -> features -> scene`，不得反向包含。

| 文件 | 层 | 说明 |
| --- | --- | --- |
| `modules/visualization/include/aerosim/visualization/features/StreamlineProcessor.h` | features | ⑦ 接口 |
| `modules/visualization/src/features/StreamlineProcessor.cpp` | features | `vtkStreamTracer` 封装 + 6 条筛选 |
| `modules/visualization/include/aerosim/visualization/features/TrajectorySet.h` | features | ⑧ 数据形态 |
| `modules/visualization/src/features/TrajectoryReader.cpp` | features | ⑧ 读取 |
| `modules/visualization/src/features/TrajectoryProcessor.cpp` | features | ⑧ 计算（P2） |
| `modules/visualization/tests/StreamlineTests.cpp` | tests | ⑦ 单测 |
| `modules/visualization/tests/TrajectoryTests.cpp` | tests | ⑧ 单测 |

同时需要**追加**（不修改既有语义）：

- `FeatureProcessor.h`：**只需**在 `FeatureError` 追加 `NoSeeds`、`SeedOutsideDomain` 两个值。原计划的 `NotSurfaceGeometry` 用现成的 `UnsupportedGeometry` 即可（P0 核对后更正），`InvalidVelocityField` 用现成的 `MissingField` 即可。`FeatureOperation` 追加 `SurfaceStreamline`、`Trajectory`。
- `modules/visualization/CMakeLists.txt`：新增两个 cpp、两个测试目标，链接 `vtkFiltersFlowPaths`（`vtkStreamTracer` 所在模块）与 `vtkFiltersCore`。

## 6. 测试计划

### 6.1 ⑦ 的表面流线断言（全部可用现有夹具）

| 用例 | 输入 | 断言 |
| --- | --- | --- |
| 平面上单调推进 | `y_plus` 面 + Velocity(1,0,0) | 线数 >= 1；x 严格单调递增；`terminations` 全为 `OutOfDomain` |
| 出域即停 | 同上 | 所有点 x <= 1.0 + 1e-9 |
| 贴面无穿透 | `y_plus` 面 | 所有点 y 等于面上 y（1e-9 容差） |
| 法向平行退化 | `x_plus` 面 + Velocity(1,0,0) | 线数 0，`rejected` 含 `parallelToSurfaceNormal` 或 `zeroLength` |
| 非法输入 | 传入未切片的 3x3x3 体网格 | 返回 `UnsupportedGeometry`，message 指向 2D 物面 |
| 缺字段 | 字段名写 `Nope` | 返回 `MissingField` |
| 边界法向投影 | 自造平面 + v=(1,0,5) | 点 z 恒为 0（复现 3.1 表） |

读取夹具时必须沿用 PR #1 的 `ReadAllScalarsOn()` + `ReadAllVectorsOn()`，否则 Velocity 载不进来（`PostProcessTestData.md` 第 17 行）。

### 6.2 ⑧ 的断言

- 读取路径：用 ⑦ 产出的轨迹写 VTP 再读回，round-trip 后点/线数与 `Time` 数组一致。
- 显示数据契约：`TrajectorySet` 缺 `Time` 或 `ParticleId` 时报 `MissingField`；缺 `TerminationReason` **不报错**，按 `Unknown(-1)` 记录（P2 实测后修正：终止原因是可选元数据，缺它不影响动画面，报错会误拒合法的第三方轨迹文件）。
- 计算路径（P3，**已完成**）：用代码内构造的 3×3×3 常速时序帧（`TrajectoryProcessorTests.cpp` 的 `frame()` / `threeFrames()`），断言：单种子得单路径、点沿 x 严格递增、起止时间等于积分窗口、结果能被 P2 读回、`Time`/`ParticleId`/`SeedId`/`TerminationReason` 四个数组齐备且每个 cell 都是 polyline。第 3.3 节「需要速度随 t 变化的夹具」的前提**已被证伪**——恒定速度足以验证积分正确性，真正的问题是当时用错了滤波器（见风险 1）。

## 7. 分阶段实施

| 阶段 | 内容 | 依赖 | 预估 |
| --- | --- | --- | --- |
| **P0** | ~~装 VTK 8.2.0 Debug~~ **已完成**；~~从 PR #1 分支切 `feature/streamline-trajectory`~~ **已完成** | — | 完成 |
| **P1** | ~~⑦ `StreamlineProcessor` + 6 条筛选 + `StreamlineTests`~~ **已完成** | P0 | 完成 |
| **P2** | ~~⑧ `TrajectorySet` + 读取适配器 + 组织/显示数据契约 + 测试~~ **已完成** | P1（复用 FeatureGeometry） | 完成 |
| **P3** | ~~⑧ 非定常计算~~ **已完成** | P2 | 完成 |
| **P4** | 场景接入（`scene/`）与 UI 种子交互 | PR #1 合并 + `scene/` 落地 | 待定 |
| **P5** | 与 ⑭ 导出联调 | P1 | 0.5d |

**P0 已完成的实际结果（供 P1 直接依赖）**：

| 项 | 值 |
| --- | --- |
| 分支 | `feature/streamline-trajectory`，从 PR #1 分支 `feature/slices-contours-isosurfaces-export-wzy`（**bb34fd3**）切出 |
| VTK | `8.2.0` Debug，自源码编译，装在项目内 `.tools\vtk-install`（225 MB）。**VTK 官方不提供 Windows C++ 预编译 SDK**，只能自编译 |
| modules/visualization 独立构建 | 6/6 目标通过 |
| PR #1 既有测试 | `aerosim_spatial_features_test` **9/9 通过**（VS2022 v142 + Qt 5.12.12 + VTK 8.2.0 Debug） |
| PR #1 的 `CMakeLists.txt` | **不含 `vtkFiltersFlowPaths`**，P1 必须在 `find_package` 的 COMPONENTS 里追加 |
| PR #1 的 `FeatureError` | 已够用：`NotSurfaceGeometry` 用现成 `UnsupportedGeometry`，`InvalidVelocityField` 用现成 `MissingField`，只需再加 `NoSeeds`、`SeedOutsideDomain` |
| 导出通道 | `FeatureProcessor::snapshot()` 注释已明确 "Accept externally generated lines/surfaces (e.g. streamlines) for export"，⑦ 输出可直接走 ⑭ |

**P0 期间发现的坑（P1 会踩，先记下）**：VTK 8.2 的 CMake 变量名与 VTK 9 完全不同——`Module_vtkFiltersFlowPaths`（非 `VTK_MODULE_ENABLE_VTK_FiltersFlowPaths`）、`VTK_Group_Qt`（非 `VTK_GROUP_ENABLE_Qt`）、`BUILD_TESTING`（非 `VTK_BUILD_TESTING`）。传错名字**不报错**，只在日志末尾给一句 dev warning，结果是缺模块的 VTK。

**P1 已完成的实际结果**：

| 项 | 值 |
| --- | --- |
| 新增文件 | `modules/visualization/include/aerosim/visualization/features/StreamlineProcessor.h`（118 行）、`src/features/StreamlineProcessor.cpp`（707 行）、`tests/StreamlineTests.cpp`（328 行） |
| 改动文件 | 模块 `CMakeLists.txt`（加 `vtkFiltersFlowPaths` 组件、加源文件、加 `aerosim_streamline_test` 目标与 ctest 条目）、`FeatureProcessor.h`（加 `NoSeeds`/`SeedOutsideDomain`）、`FeatureProcessor.cpp`（`featureErrorName` 补两个 case） |
| 构建 | 独立构建 9/9 目标通过，零错误零警告 |
| 测试 | `aerosim_streamline_test` **9/9 通过**；PR #1 的 `aerosim_spatial_features_test` 仍 **9/9 通过**（无回归）；`ctest` 2/2 通过 0.84s |
| 头文件调整 | 删除了草案里的 `StreamlineContext`（含跨帧复用 `vtkCellLocator`）。VTK 8.2 的 `SetCellLocatorPrototype` 只做 `NewInstance` 复制，**不复用已构建的加速结构**，留着等于假承诺 |

**P1 实现期间核实并固化的 VTK 8.2 行为（后续阶段直接依赖，不必再读源码）**：

| 事实 | 出处 |
| --- | --- |
| 表面投影**只对 point locator 生效**；用 cell locator 时 VTK 打一条 warning 并静默关掉投影 | `Filters/FlowPaths/vtkStreamTracer.cxx:702-718` |
| 投影数学是「先投影到切平面、再归一化」（`f -= n*dot(n,f)` 后 `Normalize`），所以步长按归一化速度走 | `Common/DataModel/vtkAbstractInterpolatedVelocityField.cxx:147-189` |
| **点数 ≤1 的线被静默丢弃**，域外种子在输出里毫无痕迹 ⇒ 判域必须在调 tracer 之前自己做 | `vtkStreamTracer.cxx:1136-1145` |
| 每条 polyline 一个 cell，`ReasonForTermination`/`SeedIds` 是 **cell 数组**，索引 == 线索引 | `vtkStreamTracer.cxx:1179-1186` |
| `BOTH` 方向时 `SeedIds` 长度翻倍，后半是 `numSeeds+i` | `vtkStreamTracer.cxx:330-338` |
| `vtkCellLocator::FindCell` 的 `tol2` 参数是 `vtkNotUsed`，**传容差不生效**；判「点是否在面上」必须用 `FindClosestPoint` + `dist2` | `vtkCellLocator.cxx:1671-1673` |
| `MinimumIntegrationStep`/`MaximumIntegrationStep` 传 0 表示「让 VTK 自适应」 | `vtkStreamTracer.cxx:257-296` |
| `vtkCell` **没有** `ComputeNormal`，只有 `vtkPolygon`/`vtkTriangle` 的静态版本 ⇒ 法向要自己叉积 | `Common/DataModel/vtkCell.h` |

**P1 测试与夹具说明**：⑦ 的 9 组断言全部用**自建曲面**——从 `cube_t00.vtk` 的 3×3×3 结构点网格手工抽出 y=-1 / y=+1 两个平面（`tests/StreamlineTests.cpp` 的 `wall()`），不依赖 `mesh.json` 的 `boundaryId` 读取（那属于 ① 的职责，见风险 4）。其中「投影去掉法向分量」一组把壁面速度改写成 `(1,5,0)`，若投影失效线会立刻冲出 y 平面，因此该断言真正验证了投影而非仅碰巧满足。

**P2 已完成的实际结果**：

| 项 | 值 |
| --- | --- |
| 新增文件 | `modules/visualization/include/aerosim/visualization/features/TrajectorySet.h`（75 行）、`src/features/TrajectoryReader.cpp`（231 行）、`tests/TrajectoryTests.cpp`（289 行） |
| 改动文件 | 模块 `CMakeLists.txt`（加 `TrajectoryReader.cpp`、`aerosim_trajectory_test` 目标与 ctest 条目）。**未改动** `FeatureProcessor.h`：`MissingField`/`UnsupportedGeometry`/`UnsupportedFormat`/`NonFiniteData` 均已存在，P2 不需要新错误码 |
| 构建 | 独立构建 6/6 目标通过，零错误零警告 |
| 测试 | `aerosim_trajectory_test` **9/9 通过**；`aerosim_streamline_test` 9/9、`aerosim_spatial_features_test` 9/9 无回归；`ctest` **3/3 通过** 1.22s |
| 读取路径 | `.vtp`（`vtkXMLPolyDataReader`）与 legacy `.vtk`（`vtkDataSetReader`）均支持，按后缀分派；其它后缀 `UnsupportedFormat`，不做猜测 |
| Unicode 路径 | 沿用写端的做法：Qt `QFile` 读全字节 → `SetInputString` 交给 VTK。**不经过 VTK 的窄字符文件名接口** |

**P2 实现期间核实并固化的行为（P3 直接依赖）**：

| 事实 | 出处 |
| --- | --- |
| legacy `.vtk` 读取器**默认丢弃数组**，不调 `ReadAllScalarsOn()`/`ReadAllVectorsOn()`/`ReadAllFieldsOn()` 会连 `Time` 都读不到，症状是「文件明明有数据却报字段缺失」 | `IO/Legacy/vtkDataReader.h:252-308`；`docs/PostProcessTestData.md:17` |
| `vtkXMLReader::SetInputString(const std::string&)` 与 `vtkDataReader::SetInputString(const char*, int)` 是两条不同签名，legacy 端必须显式传长度 | `IO/XML/vtkXMLReader.h:69-72`、`IO/Legacy/vtkDataReader.h:104-109` |
| `vtkParticleTracerBase` 产出的是**点云 + `SetVerts`**，不是 polyline；`ParticleId`/`ParticleAge` 是它自己的数组名，但 cell 级的 `TerminationReason` 它**不产出** ⇒ P3 必须自行按 `ParticleId` 分组组装 cell 数组 | `Filters/FlowPaths/vtkParticleTracerBase.cxx:762-797` |
| `FeatureGeometry` 构造函数是 private + `friend FeatureProcessor`，任何模块外代码都只能经 `FeatureProcessor::snapshot(vtkPolyData*)` 构造 | `FeatureProcessor.h:45-56` |
**P3 已完成的实际结果**：

| 项 | 值 |
| --- | --- |
| 新增文件 | `modules/visualization/include/aerosim/visualization/features/TrajectoryProcessor.h`（49 行）、`src/features/TrajectoryProcessor.cpp`（508 行）、`tests/TrajectoryProcessorTests.cpp`（约 250 行） |
| 改动文件 | 模块 `CMakeLists.txt`（加 `TrajectoryProcessor.cpp` 源文件、`aerosim_trajectory_processor_test` 目标与 ctest 条目）。**未改动** `FeatureProcessor.h`：`NoSeeds`/`SeedOutsideDomain` 在 P1 已加，P3 不需要新错误码 |
| 构建 | 独立构建 7/7 目标通过，零错误零警告 |
| 测试 | `aerosim_trajectory_processor_test` **9/9 通过**；`aerosim_trajectory_test` 9/9、`aerosim_streamline_test` 9/9、`aerosim_spatial_features_test` 9/9 无回归；`ctest` **4/4 通过** 1.11s |
| 接口 | `TrajectoryProcessor::integrate(const std::vector<TrajectoryFrame>&, const TrajectoryParameters&)` → `TrajectorySet`。`TrajectoryFrame{vtkDataSet* data; double time;}`，**复用 `StreamlineSeeds`** 让 ⑦⑧ 共用一种种子类型 |
| 通道 | 直接产出 `TrajectorySet`（经 `FeatureProcessor::snapshot` 造 `FeatureGeometry`），**不需要落临时 VTP 再读回**。原计划的中转链路已被证明多余 |
| 步长 | **不暴露**。`vtkParticleTracerBase` 的 `IntegrationStep`/`MaximumError` 是 protected 成员且无 setter（`vtkParticleTracerBase.h:502-503`），暴露就是假承诺 |

**P3 实现期间核实并固化的 VTK 8.2 行为（后续阶段直接依赖，不必再读源码）**：

| 事实 | 出处 |
| --- | --- |
| **必须用 `vtkParticlePathFilter`，不能用 `vtkParticleTracer`**：后者只输出点云 + `SetVerts`，不建折线。这是第 3.3 节「三帧恒定速度不可靠」的真正原因，恒速本身无害 | `vtkParticleTracerBase.cxx:797,1517`；`vtkParticlePathFilter.cxx:116-134` |
| **`ErrorCode` 数组在输出里恒为 0**，原定的「ErrorCode→StreamlineTermination 映射表」方案不成立。所有赋值分支都在 `ParticleHistories.erase(it)` 之后，而 `AddParticle` 只在 `particle_good` 为真时调用 ⇒ 终止原因只能从末点几何与时间反推 | `vtkParticleTracerBase.cxx:1073,1126,1182,1470,1476,1482,1504,1517-1523` |
| **出域与停滞的粒子被静默移除**，不留任何输出点。`SendParticleToAnotherProcess` 恒返回 true，`vtkParticlePathFilter` 未重写 ⇒ 域外种子、早停轨迹都必须由调用方自行判定与解释 | `vtkParticleTracerBase.h:403-408`；`.cxx:604-632,1192-1203` |
| 种子域外时只打一条 `vtkDebugMacro`，不 push 也不留痕 ⇒ **判域必须在调 tracer 之前**（与 P1 同一条教训） | `vtkParticleTracerBase.cxx:604-632` |
| `ParticleId` 是**通过 `TestPoint` 的种子的压缩序号**，不是原始种子下标 ⇒ 调用方若过滤了种子，必须自己维护回原始序的映射 | `vtkParticleTracerBase.cxx:682-693` |
| **非定常只能接受 point 关联的速度数组**。`vtkParticleTracerBase` 调的是单参 `SelectVectors(name)`，`VectorsType` 恒为 `FIELD_ASSOCIATION_POINTS`；cell 关联数组会让每次求值报 "Can't evaluate dataset!"。对比：P1 的 `vtkStreamTracer` 用双参版本，两种关联都支持 | `vtkAbstractInterpolatedVelocityField.cxx:49,108,130`；`vtkTemporalInterpolatedVelocityField.h:94-95` |
| 单帧输入只 `vtkWarningMacro` 不报错；起止时间超范围也只 `vtkErrorMacro` 并在 `RequestInformation` 里**静默夹取** ⇒ 这些前置校验必须由调用方自己做 | `vtkParticleTracerBase.cxx:239-252,288-292,301-319` |
| 输出端口若声明抽象基类 `vtkDataSet`，执行管线实例化输出对象时崩 `0xC0000005`；必须声明具体类名（`.cpp` 里用 `frames[0].data->GetClassName()` 动态取） | 实测 |
| `TIME_STEPS()` 必须用数组形式 `info->Set(key, &times[0], n)`；变参形式超过约 20 个参数会报 no overloads | 实测 |

**P3 的三处终止原因判定（`classifyTermination`）**：末点时间到达 `terminationTime`（容差 `1e-9*max(1,|t|)`）→ `OutOfLength`；末点速度 ≤ `terminalSpeed` → `Stagnation`；末点在 bbox 某面 `sqrt(sum(span^2))*1e-3` 以内 → `OutOfDomain`；都不满足 → `Unknown`。**不设 `UnexpectedValue`/`NotInitialized`**，因为 `vtkParticlePathFilter` 没有可观测信号支持它们，硬造等于编数据。

**P3 测试的一处踩坑（值得记住）**：`TrajectoryFrame` 只存裸指针，最初测试里写 `f.data = frame(...)` 让临时 `vtkSmartPointer` 在语句结束即析构，测试直接段错误（`0xC0000005`）。改为 `Fixture` 结构同时持有 `std::vector<vtkSmartPointer<vtkStructuredPoints>> owned` 与 frames。**这不是实现 bug，但任何调用方都必须保证数据集在 `integrate` 返回前存活**（头文件第 17-18 行已写明）。

**分支策略（重要）**：PR #1 未合并，而 ⑦⑧ 必须依赖它提供的 `FeatureGeometry`/`FeatureStatus`/`FeatureJob` 与 `modules/visualization/CMakeLists.txt`。因此 `feature/streamline-trajectory` 应**从 PR #1 分支 `feature/slices-contours-isosurfaces-export-wzy` 切出**，而不是从 `main` 切出。若 PR #1 先合并，则改为从 `main` 切出。

## 8. 风险与未验证项

1. ~~**VTK 8.2 与 9.5 的 API 差异**~~ —— **已解除**：P1 的表面流线、P2 的读写适配器、P3 的非定常积分均已在 VTK 8.2.0 Debug 上实测通过。第 3.3 节「三帧恒定速度导致 tracer 不可靠」的归因**已被 P3 证伪**：恒速场照样能积分出轨迹，真正的原因是当时用了 `vtkParticleTracer`（只产点云 + `SetVerts`），必须用 `vtkParticlePathFilter`（自己建 polyline）。
2. ~~**本机无 VTK C++ 库**~~ —— **已解除**：P0 已把 VTK 8.2.0 Debug 编译到项目内 `.tools`，且 PR #1 的 9 组测试在该构建上通过，证明工具链可用。
3. **Windows 仅 VTK Debug**（`docs/DevelopmentEnvironment.md` 第 22 行：Release 探针运行时报 `0xc0000374`）。
4. **物面选择的上游尚不存在**：`mesh.json` 的 `boundaryId` 目前无任何 C++ 代码读取，属于 ① 网格导入的范围。P1 的测试可以自建曲面绕过，但正式使用时物面从哪来需随 ① 一并定。
5. **⑧ 的格式未定** —— **读取侧已收窄**：P2 把可接受的输入定义为「polyline + 必需点数组 `Time`/`ParticleId` + 可选 cell 数组 `TerminationReason`」，`.vtp`/`.vtk` 均可读。**仍未解决的是真实样本**：仓库至今没有任何来自真实求解器的轨迹文件，P2 的 9 组断言用运行时构造的假数据（`makeTrajectories()`），P3 的 9 组用自建的 3×3×3 常速帧。真实文件的数组命名、时间单位、是否带 `SeedId` 仍是假设。

## 9. 需要评审拍板的决策点

| 编号 | 决策 | 选项 | 建议 |
| --- | --- | --- | --- |
| **D1** | 迹线是 读取 还是 计算 | A 只读取 / B 只计算 / C 两者都要（内部解耦） | **C，已按 C 落地**：P2 交付读取（`TrajectoryReader`），P3 交付计算（`TrajectoryProcessor`），两侧产出同一个 `TrajectorySet`，显示端无需知道数据来源。原建议里「先读取后计算」的排期理由是夹具不足，该前提已被 P3 证伪，但分阶段交付本身仍然成立 |
| **D2** | 种子交互形态优先级 | 点 / 线 / 面 三者排序 | **面优先**（最贴合表面流线语义，且与切片结果天然衔接），点次之，线最后 |
| **D3** | 4.2 的 6 条筛选规则是否成立 | 采纳 / 调整阈值 / 删减 | 采纳结构，**规则 2 与 6 的默认值改为显式必填**，避免魔数 |
| **D4** | 场景接入是否进本次范围 | 进 / 不进 | **不进**。`scene/` 未实现，硬接会违反 每个功能只操作自己的 sceneObjectId 的所有权约定 |
| **D5** | 是否安装 VTK 8.2.0 Debug | 装（约 1-2 GB，可放 `.tools\`） / 不装并标注未验证 | **装**，否则 P1/P2 无法自证 |
| **D6** | 物面来源 | `mesh.json` boundaryId / 切片结果 / 两者 | **两者都支持**，接口只收 2D 数据集，来源由调用方决定 |

## 10. 一句话总结

⑦ 的核心风险比预想低：**表面投影和终止原因都由 VTK 直接给出**，我们要做的是 2D 物面校验 + 6 条筛选规则 + 结果封装。⑧ 的核心风险在于**夹具无法验证跨帧追踪**，因此建议先交付 读取与数据组织，把 非定常计算 作为独立阶段等夹具就绪。
