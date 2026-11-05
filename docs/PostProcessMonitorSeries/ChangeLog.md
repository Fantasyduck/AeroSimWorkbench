# 监控序列实现 — 修改日志

对应规划：[docs/PostProcessMonitorSeriesPlan.md](../PostProcessMonitorSeriesPlan.md)
执行汇总：[ExecutionSummary.md](ExecutionSummary.md)
分支 `postprocess/analysis-monitor-series`，基线 `1a56ca3`（上游 main，本任务开始前已硬重置到该提交）。

本文件按文件列出改动内容与原因。凡是「对应」栏写了规划条目号的，都可在规划文档里找到出处；标「超出规划」的已在执行汇总第 5 节单独列出。

## 一、新增：数据层

### 1. `modules/visualization/data/MonitorLogReader.h`（69 行）/ `.cpp`（385 行）

**对应**：规划 P0 首项、§3.1 解析要点、§3.3（不复用在线遥测契约）。

- `struct MonitorColumn{key, unit, isResidual, isAxis}`、`struct MonitorLog{datasetId, sourcePath, columns, iterationStep, timeIndex, values, warnings, outerIterationProgression, innerIterationProgression}`。
- `canRead(path)` / `read(path, MonitorLog*, QString*)` / `readCsv(QByteArray, sourcePath, ...)` / `datasetIdForPath` / `looksLikeResidual` / `unitForColumn` 全部为静态函数，无实例状态。
- **列集合不写死**：按表头列名索引，不假设 17 列。原因：规划 §3.1 实测另一份公开样例多出 `RefForce` 列，硬编码列名会崩。新增列只进 `warnings`，删除列才报错。
- 表头先 `trim` 再去成对引号。原因：该文件列名带引号且引号内有对齐空格（`    "rms[Rho]"    `），不处理则 key 匹配不上。
- 迭代步回退顺序 `Outer_Iter` → `Inner_Iter` → 行序号。原因：该 SU2 样例 `Time_Iter`/`Outer_Iter` 恒为 0，真正推进的是 `Inner_Iter` 0→325；只认 `Outer_Iter` 会得到一条完全扁平的曲线。
- 空文件、含 `\0` 的二进制、含 U+FFFD 的非 UTF-8、无数据行，全部返回可读错误。对应规划 §5.5「不允许静默失败」。
- **实时链路要复用的三个方法（后补，纯新增）**：`static MonitorColumn classifyColumn(const QString &columnKey)` 让实时源与离线读对同一表头给出完全相同的分类，避免第二份规则漂移；`static QStringList splitCsvLine(const QString &line)` 与 `static QString trimmedField(const QString &raw)` 原先只在匿名 namespace 内，实时源需要同一套引号/填充处理，提为公有静态方法后两条路径共用一段代码。原因见 §七 第 5 条。

### 2. `modules/visualization/analysis/MonitorSeriesModel.h`（65 行）/ `.cpp`（189 行）

**对应**：规划 P0 第二项、§4「analysis 不依赖 QWidget」。

- `setLog` / `isValid` / `log` / `sourcePath` / `datasetId` / `rowCount` / `axisAvailable(MonitorAxis)` / `axisAvailable(MonitorAxis, QString*)` / `axisLabel` / `points(key, axis)` / `isResidual` / `unit` / `suggestedScale(单列)` / `suggestedScale(QStringList)` / `residualColumns` / `coefficientColumns` / `seriesColumns` / `warnings`。
- `points()` 跳过非有限值。原因：不能把 NaN 传给绘图库，否则整条曲线消失且无提示。
- `axisAvailable(MonitorAxis, QString *reason)` 是唯一的轴退化判定入口，三种原因文本：
  - `尚未导入监控日志。`
  - `该文件没有 Time_Iter 列，无法按时间索引显示。`
  - `该文件为定常推进，时间索引恒为 %1，不能作为横轴；请改用迭代步。`

  对应规划 §5.3「不允许出现选了没反应」。
- 只依赖 `aerosim_monitor_data` 与 `Qt5::Core`，不链 VTK、不链 Widget。对应规划 §4「不引入 VTK」。

### 2b. `modules/visualization/analysis/MonitorCurveFit.h`（68 行）/ `.cpp`（271 行）

**对应**：用户 m09044 选定的「真拟合：散点保留、拟合线叠加」。

**为什么必须自己写**：Qwt 7.5.2 不含任何最小二乘拟合器。`src/plot` 下只有 `qwt_curve_fitter`、`qwt_spline_curve_fitter`、`qwt_weeding_curve_fitter` 与 `qwt_spline*`，全部是**插值**——数学上必须穿过每一个采样点。所以拟合算法放在 analysis 层（纯数学，不依赖任何绘图库）。

- 公有：`enum class MonitorFitMethod { None, Linear, Polynomial, MovingAverage }`；`struct MonitorFitResult { QVector<QPointF> points; double rSquared; int degree; int window; bool ok; QString note; }`；`MonitorCurveFit::fit(samples, method, degree, window)`、`methodName`、`maxDegree()`（返回 9）。
- 多项式：**x 先中心化并按 max|x-xMean| 缩放到 [-1,1]** 再列法方程。迭代步是数百量级，直接在原始 x 上求解精度损失过大。法方程用高斯消元 + 部分主元，主元 < 1e-12 判奇异。
- 阶数 `qBound(1, degree, qMin(maxDegree(), n-1))`，被调整时写 `note`「阶数已从 %1 调整为 %2」，不静默换模型。
- 移动平均：**居中窗口**，两端**收缩窗口而不丢点**（`from=qMax(0,i-half); to=qMin(n-1,i+half)`），窗口强制奇数、下限 3。
- 退化输入一律返回 `ok=false` 并给中文 `note`：样本 < 3、所有 x 相同、方法为 None。不画假趋势。
- `fit()` 先过滤非有限值并按 x 升序排序——模型从不承诺输入有序。
- R² 在总方差 <= 0 时特判 1.0/0.0，避免除零。

### 2c. `modules/visualization/data/MonitorLiveSource.h`（74 行）

**对应**：用户 m10071「IO 模块应该还有一个实时的读入数据……实时更新绘图」、m10110「输入类型还不清楚，先预留接口」。

- 三个类型：`struct MonitorUpdate{columns, values([列][行]), iterationStep, timeIndex, warnings, restarted, axisFromRowIndex}` + `rowCount()`/`isEmpty()`；`class MonitorLiveSource` 抽象端口；`enum class MonitorSourceKind` 预留给未来的来源枚举。
- **端口刻意不继承 QObject**：`sourceId()` / `start(QString*)` / `stop()` / `isRunning()` / `poll(MonitorUpdate*, QString*)` 全是普通虚函数，纯拉取式。原因：文件增长、子进程 stdout、socket、遥测总线都能挂在后面，而数据层不必知道事件循环怎么跑，无界面环境里也能直接用。
- `MonitorUpdate` 是**行增量**而不是整份快照。原因：求解器每次迭代产出一行，整份快照要么每次重读文件、要么每次复制全部历史，两者在长跑算例上都不可接受。

### 2d. `modules/visualization/data/MonitorLogAccumulator.h`（64 行）/ `.cpp`（170 行）

**对应**：同上。这是「增量」与「完整日志」之间的唯一转换点。

- 公有：`setDatasetId` / `datasetId` / `reset` / `append(update, error)` / `log()` / `revision()` / `rowCount()`。
- 作用：把 `MonitorUpdate` 折成一份 `MonitorLog`，**让 analysis 与 charts 两层完全不需要知道数据是分批到的**——它们看到的仍是离线读取那种完整日志。
- `append()` 先 `if (update.restarted) reset();` 但**不 return**。原因：重启标记与该轮首批行常在同一次投递里到达，return 会丢掉那批行。
- 空投递（`update.isEmpty()` 且无重启、无 warning）直接返回 true，不推进 revision。原因：轮询在「暂无新数据」时是绝大多数，不能每次都当成一次更新。
- `widen()` 对首次投递之后新增的列，按已有行数回填 `numeric_limits<double>::quiet_NaN()`。原因：**填 0 会在图上画出求解器从未报过的平线**；NaN 让 `points()` 自然跳过。
- 每列行数与迭代步不一致时**整批拒绝**并报「实时更新的一列行数与迭代步不一致，已忽略本次更新。」，不半应用。原因：部分应用会让日志内部行数错位，之后所有读取都是错的。
- `axisFromRowIndex` 时 `step += startRow` 续号。原因：每轮从 0 重来会让横轴倒着走。
- `datasetId_` 跨 `reset()` 保留。原因：同一个求解器重启仍是同一份数据，曲线 id 不该因此改变（`curveId = datasetId + "|" + key`）。
- `outerIterationProgression` / `innerIterationProgression` 每轮用 `progressionOf()` 重算。原因：决定哪列当横轴的正是这个值，它在运行中会变。

### 2e. `modules/visualization/data/MonitorFileTailSource.h`（60 行）/ `.cpp`（253 行）

**对应**：同上，作为可运行的参考实现。

- 构造 `MonitorFileTailSource(const QString &path)`，`sourceId()` 取 `MonitorLogReader::datasetIdForPath(path)`，与离线读取同名同 id。
- `poll()` 只读**追加的字节**（不是每次重读整份文件）；`consumedBytes()` 暴露读位置供测试断言。
- 游标语义：`offset_` 只记已读到的文件位置，未解析完的字节全部留在 `pending_`。两个量混用会导致首行丢失。
- 半行永不解析：`raw = pending_.split('\n'); pending_ = raw.last();` 只解析前 n-1 行。原因：正在被写入的行解析出来是一批假值。
- 文件变短即判定重写：`if (size < offset_)` → 归零、清 pending、重置表头状态、`update->restarted = true`、warning「历史文件被重写，已按新内容重新读取。」
- 表头与数据行都用 `MonitorLogReader::splitCsvLine` + `trimmedField` + `classifyColumn`。原因见 §七 第 5 条：自己写一遍归一化会让两条路径的列键不一致。

## 二、新增：绘图层

### 3. `modules/visualization/charts/MonitorChartView.h`（458 行）/ `.cpp`（1619 行）

**对应**：规划 §5.3（曲线页与横轴）、§5.4（叠加与双轴分组）、§9 风险表「曲线视图与图表库之间留一层薄封装」。

**公有 API（全部为本任务新增，未改动任何既有接口）**：

`setAxisLabel`、`setChartTitle`、`chartTitle`、`setCurves`、`appendPoints`、`clearCurves`、`static curveId`、`static makeCurveId`、`curveIds`、`groupKeys`、`curveIdsInGroup`、`setLabelResolver`、`legendLabels`、`setCurveFactory`、`setCurveStyle`、`setCurveLabel`、`curveStyle`、`applyStyle`、`setSampleIntervalForGroup`、`setSampleIntervalForCurves`、`setSelectedCurve`、`selectedCurve`、`setSelectedPoint`、`selectedPointCurve`、`selectedPointSample`、`sampleIndexAt`、`selectAt`、`setPointBudget`、`pointBudget`、`visibleCurveCount`、`drawnPointCount`、`drawnSampleCount`、`pointSeriesCount`、`lineSeriesCount`、`axisCount`、`axisSummaries`、`statusText`、`sceneItemCount`、`chartBuildCount`、`savePng`、`fitSeriesCount`、`fitResult`；信号 `curveClicked`、`pointClicked`、`curveStyleChanged`、`curveSelectionChanged`。

**数据结构**：`MonitorCurve`、`MonitorCurveStyle`、`MonitorAxisSummary`、`MonitorPointContent`、`MonitorFitStyle`、`StylePatch`、`enum class EditScope`、`enum class MonitorPointShape`（20 项）、`MonitorInterpolation`、`MonitorFitMethod`、`MonitorAxis`、`MonitorScale`。全部是纯 Qt 类型（`QColor`/`QPointF`/`QString`），**不含任何绘图库类型**——唯一例外是 `CurveFactory` 返回绘图库的曲线对象，因为自定义拟合器必须拿到该对象；这正是规划 §9「换库只改实现」预留的那层封装。

**关键实现决策**：

| 决策 | 原因 |
| --- | --- |
| 每次 `setCurves()` 新建 plot 再 delete 旧的，`chartBuildCount_` 自增 | 在旧 plot 上 detach 轴会留下 scale widget，导致轴标签重影。实测：全新视图 76 个场景图元，重绘后涨到 159/260 |
| `sceneItemCount()` 作为可失败回归断言 | 用来钉住上面那条重影缺陷 |
| marks 与 joining 是两个曲线对象：marks 用 `NoCurve` + 符号，joining 用 `Lines` | 满足规划 §5.3「点为主粒度、连线可选」；`MonitorInterpolation::None` 时只有 marks |
| 选中态靠线宽/标记放大（`kSelectedCurveWidth = 3.5`、`kSelectedMarkerScale = 1.6`）而非换色 | 颜色是调用方的编码信息，不能让选中态覆盖它 |
| 拾取在**画布像素空间**找最近点（`kPickRadiusPx = 12.0`，平方比较） | 不同分组共用画布但 Y 刻度不同，数据空间距离不可比 |
| `savePng` 先 `resize(target)` 再渲染，并内缩 10px 手动调 `render()` | ① 渲染器在 plot 尺寸为 0 时直接 return；② `renderTo()` 把整张图当绘图矩形，装饰性文字紧贴边框会被切掉——实测顶部标题丢了首字、右轴标题伸出右缘 |
| 每侧最多两组量级，`constexpr int kMaxMagnitudeGroups = 2;`，超出者**整组不画**并在状态栏列出名字 | 绘图库每侧只有一条轴（`QwtAxis::Position` 仅 YLeft/YRight/XBottom/XTop）。共享刻度正是分组机制要防的压平，所以宁可不画也不合并 |
| 拟合层是**第三个** `QwtPlotCurve`（`CurveItems::fitted`），与 marks、joining 并列 | 拟合与「连接」是正交的两件事：连接必须穿过每个采样点，拟合通常一个都不穿过。二者可独立开关 |
| 拟合默认虚线、且 `setItemAttribute(QwtPlotItem::Legend, false)` | 拟合是对数据的概括而非一次测量，不能与真实采样序列混淆，也不能让同一物理量在图例里出现两次 |
| 拟合只用 `drawnSamples()` 抽稀后的样本，且不外推 | 拟合必须描述画布上真正画出来的东西；超出采样 x 范围的部分没有数据依据 |
| 改拟合方法/阶数/窗口走 `rebuildFitSeriesFor()` 重建该层；改线型/颜色走 `applyLiveStyle()` 不重建 | Qwt 把采样点存在 item 上，方法一变就必须重建；只改画笔则没有重建的理由 |
| `MonitorPointShape` 开放全部 19 种可绘制形态（含 Path / Pixmap / Graphic / Svg） | 用户 m08687 要求「全部开放」。四类需要外部绘制内容的形态由 `MonitorPointContent` 承载，**只收 Qt 类型**（`QPainterPath` / `QImage` / `QByteArray`）：Graphic 在 Qwt 侧要 `QwtGraphic`，由实现内部转换，公有头仍不出现绘图库类型 |
| `MonitorPointShape::None` 映射为 `setSymbol(nullptr)` | 补上「纯折线、不要点」这个此前无法表达的形态：`MonitorInterpolation::Linear` 同时画点画线，无法只留线 |
| 采样以**逐曲线间隔**表示（`MonitorCurve::sampleInterval`，每 N 点画 1 个），不是「点数上限」 | 间隔对 79 行的夹具和 10 万行的日志是同一句话；点数上限要求用户先知道文件有多少行。全局 `setPointBudget()` 保留但降级为防病态文件的安全帽，与间隔取较大步长 |
| `StylePatch` 每个字段成对出现（`hasXxx` + 值），组级编辑只写用户真正动过的那一项 | 若整份样式一起写，「把这一组点调大」会顺手把整组刷成同一种颜色，而组内正是靠颜色区分成员的 |
| 可见性与采样间隔改动走 `setCurves()` 重建；颜色/线型/点形/拟合线型走 `applyLiveStyle()` 不重建 | Qwt 把采样点存在 item 上、把可见性烙进 item 在图例与轴上的存在，这两类东西变了 item 本身就得重建 |
| 间隔存在视图里（`sampleIntervalsById_`）而不是只存在曲线上 | 隐藏的曲线没有活的 item，间隔从图上读不回来；调用方重建曲线列表时也不必自己记住 |
| 图内标题可由调用方用 `setChartTitle()` 指定，空串才回落到内置摘要 `%1 条序列 · %2 条纵轴`；改标题只调 `plot->setTitle()`，**不重建图** | 标题应当说明画的是什么物理量（残差、系数），统计数字属于 `statusText()`。不重建是为了保住当前选区与缩放；`builtInTitle_` 缓存上一次 `setCurves()` 本会用的文字，清空调用方标题时无需重建即可恢复 |

**状态栏文案契约**（测试断言依赖，改动会打破 `MonitorChartRenderTest`）：
- `已绘制 %1 条序列 / %2 个数据点，%3 条纵轴。`
- 分组串 `%1 → %2%3（%4 条）`
- 未连接时 `全部序列仅绘制离散采样点，未连接。`
- 抽稀时 `已按每 %1 个采样点取 1 点抽稀 %2 条序列，末点保留。`
- 无曲线时 `尚未导入监控序列。` / `当前没有勾选任何监控序列。`
- 对数降级时 `含非正值无法取对数、该组已按线性绘制：…`
- 超出轴上限时 `超出两条纵轴上限、未绘制的分组：%1。`
- 拟合成功时 `拟合：%1（%2，R²=%3）。`
- 拟合失败时 `未能拟合：%1。`

### 4. `MonitorChartView::appendPoints()` —— 不重建的增量通道

**对应**：实时链路在绘图侧的落点。

```cpp
bool appendPoints(const QString &curveId, const QVector<QPointF> &points);
```

- 不用 `setCurves()` 重画。原因：重建会丢掉缩放、选中与用户刚改过的样式，而求解器每迭代一行都付不起这个代价。
- 找不到 `curveItemsById_[id].marks` 或 `curves_` 里没有该 id 时返回 false，调用方退回 `setCurves()`。原因：隐藏的曲线没有活的 item，此时追加无处可落，必须让调用方知道。
- 成功时 `curves_[i].points += points` 与 `curvesById_[id].points += points` **两处都写**。原因：只写一处会让下一次 `setCurves()` 静默丢掉这些数据。
- 同步 `marks` / `joining` / `fitted` 三层，再 `rescaleForCurve()` + `replot()`。
- `rescaleForCurve()` 的 Y 范围按**同 axisId 的整组**可见曲线取。原因：轴是组共享的，只按单条曲线定范围会挤掉同组其它曲线。
- 状态栏改为 `statusBase_` + 「已接收实时追加 %1 行，未重建图表。」；`statusBase_` 在 `setCurves()` 末尾保存，之后每次追加只替换尾部。
- 新增私有常量与成员：`liveAppendedRows_`、`statusBase_`、`rescaleForCurve()`。

## 三、修改：模块顶层 CMake

### 5. 三个 CMake 目标

| 文件 | 内容 |
| --- | --- |
| `modules/visualization/data/CMakeLists.txt`（15 行） | `aerosim_monitor_data` STATIC，只链 `Qt5::Core` |
| `modules/visualization/analysis/CMakeLists.txt`（12 行） | `aerosim_monitor_analysis` STATIC，链 `aerosim_monitor_data` + `Qt5::Core`。含 `MonitorSeriesModel` 与 `MonitorCurveFit` |
| `modules/visualization/charts/CMakeLists.txt`（65 行） | `aerosim_monitor_charts` STATIC，链 `aerosim_monitor_analysis` + `Qt5::Widgets` + Qwt |

`charts/CMakeLists.txt` 的 Qwt 查找顺序：`find_package(Qwt 7.5 QUIET)` → `Qwt::Plot`/`qwt::plot` → 否则按 `AEROSIM_QWT_ROOT` 合成 `aerosim_qwt` UNKNOWN IMPORTED 目标 → 都失败则 `FATAL_ERROR` 并给出 `-DAEROSIM_QWT_ROOT` 提示。**不写任何绝对路径**（AGENTS.md 禁止把个人绝对路径写入 CMake）。

**收尾复核时改过一处**：原实现对 Qwt 头目录只 `find_path` 了 `src/plot`，并把同一个目录塞进两个变量，等于**`src/core` 与 `src` 根都没进 include 路径**。这会让下游编译直接失败——`qwt_global.h` 在 `src/` 根、`qwt_scale_map.h`/`qwt_scale_engine.h` 在 `src/core`。改为三条独立 `find_path`（`qwt_plot.h`/`qwt_scale_map.h`/`qwt_global.h` 各认一个名字），任一缺失则条件不成立、落到 `FATAL_ERROR`，而不是等到编译期才报 `C1083`。同时删掉库搜索路径里的 `${AEROSIM_QWT_ROOT}/build-pp/qwt7`——那是把本机目录布局写进了构建脚本。
### 6. `modules/visualization/CMakeLists.txt`（8 行 → 41 行）

**对应**：规划 P0「新增两个 STATIC 目标」。

原文件是 8 行骨架（`project(... LANGUAGES NONE)` + 一句「intentionally a skeleton」注释）。改为：`LANGUAGES CXX`、C++14、`CMAKE_AUTOMOC ON`、MSVC 下 `/utf-8`；三个 `option`（`AEROSIM_BUILD_TESTS`/`AEROSIM_BUILD_CHARTS`/`AEROSIM_BUILD_MONITOR_DEMO`，全 ON，仅独立配置时声明）；

`find_package` **两次**——`Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core` 无条件，`Widgets Svg PrintSupport Concurrent` 仅在 `AEROSIM_BUILD_CHARTS` 打开时请求（Qwt 用 Svg 画符号、PrintSupport 与 Concurrent 供渲染项使用，故在模块层而不是 `charts/` 内部声明）；

`add_subdirectory` 五次：`data`、`analysis` 无条件；`charts` 由 `AEROSIM_BUILD_CHARTS` 保护；`demo` 需 `AEROSIM_BUILD_CHARTS AND AEROSIM_BUILD_MONITOR_DEMO`；`tests` 由 `AEROSIM_BUILD_TESTS` 保护并先 `enable_testing()`。因此关掉 charts 时，渲染用例整体不进构建。

原有注释「Add a CMake target only when a submodule has an independently buildable implementation and test」的前提已满足（三个目标都有独立测试），故删除。

## 四、新增：测试

### 7. `modules/visualization/tests/MonitorLogReaderTest.cpp`（698 行）

**对应**：规划 P0 单元测试要求（列数、指定列数值、列名去空格去引号、未知列告警、缺列报错、空文件报错、轴退化判定）。

关键断言：列数 17、首行 `rms[Rho]`=-1.60335846、末行 `CD`=0.01100871693、`Inner_Iter` 末值 325、`Time_Iter` 恒 0 时 `axisAvailable(TimeIndex)` 为 false 且原因含「定常」。

**`testLiveAccumulator()`（新增）**：两批 3 行 × 4 列投递 → `rowCount` 3→6、`revision` 递增、CD 末值 60.0；空投递不报错不加行；ragged 投递（删掉一列最后一个值）被**整批拒绝**且日志不变；中途新增 CEff 列 → 列数 8、row 0..5 为 NaN、row 6=7.0、row 7=8.0；`restarted` 投递 → `rowCount` 回到 3 且 CEff 消失；无迭代列 + `axisFromRowIndex` 两次投递 → `iterationStep` 为 {0,1,2,3}。

**`testFileTail()`（新增）**：只有表头 → 0 行；追加半行 → **0 行**（半行不解析）；补完整 → 恰好 1 行；再追加一行 → 只返回新增的那一行且 `consumedBytes()` 前进；无新内容 → 0 行；把文件截断回表头 → `update.restarted == true` 且 warnings 非空；`start()` 可重开、`stop()` 后 `isRunning()==false`。

**`testLiveAndOfflineAgreeOnColumnKeys()`（新增，针对 §七 第 5 条那个缺陷）**：写一份**带引号且带对齐空格**的表头（`   "Inner_Iter",   "Outer_Iter",   "CD",   "rms[Rho]"`）加两行填充数值的 CSV，分别用 `MonitorLogReader::read` 与 `MonitorFileTailSource::poll` 读，断言 ① 两条路径给出的列键序列**完全相同**、② 没有任何列键带引号、③ 括号列名 `rms[Rho]` 原样保留、④ 该列仍被分类为残差、⑤ 填充空格后的数值被正确解析。

**`testCurveFit()`（新增，14 项断言）**：直线 R²=1 且首末 x 与采样范围完全一致（不外推）；加噪后 R² 严格落在 (0,1)；抛物线在 2 阶达 R²=1、1 阶明显解释不了；阶数被夹到 n-1 且 `note` 写明被调整；移动平均覆盖首末 x 且把孤立尖峰削低；乱序输入被排序输出；两点样本与垂直样本被拒绝并给 `note`；含 NaN 的样本被跳过而非致命；`MonitorFitMethod::None` 不产出任何趋势；三种方法都有非空名字。

### 8. `modules/visualization/tests/MonitorChartRenderTest.cpp`（909 行）

新增 `testEveryMarkerShapeDraws()`：以 `pointShape = None` 的渲染结果为参考图，对 19 种可绘制形态逐个断言 ① `curveStyle(id).pointShape` 往返一致、② 渲染像素与参考图不同（即真的画上去了）。Path/Pixmap/Graphic/Svg 四种由测试自带内容（五边形 `QPainterPath`、16x16 `QImage`、一段 `viewBox="0 0 10 10"` 的 SVG 字节）。出图用 6 个采样点的 gallery 序列（`pointSize = 14.0`），每种形态一张 480x300 PNG。

**教训**：第一版用真实 79 点数据配 `pointSize 11`，断言全过但 19 张图全是叠在一起的色带，无法目视验收。验收产物必须可读，不能只满足断言。

**对应**：规划 §7 测试计划「UI smoke」层次。

断言全部走公有 API（`axisCount`/`chartBuildCount`/`sceneItemCount`/`pointSeriesCount`/`lineSeriesCount`/`legendLabels`/`statusText`/`savePng`），并**真的写出 PNG 并断言尺寸**——「画出了东西」与「算对了点」是两个独立命题。

新增三组量级（group-a/b/c）场景断言：`axisCount()==2`、`pointSeriesCount()==2`、`legendLabels().size()==2`、`statusText().contains("group-c")`。用来钉住「超出两条轴的分组不画」这条行为。

踩坑记录：第一版用 `visibleCurveCount()==2` 断言失败——它统计的是调用方输入里标了 visible 的曲线（仍是 3 条）。真正落到画布上的计数是 `pointSeriesCount()`。

新增 `testScopedStyleEdits()`（7 条曲线：4 残差 + 3 系数，两个 groupKey）：

- `setSampleIntervalForGroup("residual", 5)` 返回 4，该组每条曲线的 `drawnSampleCount` 从 79 变成 **17**，`chartBuildCount()` 加一，另一组仍是 79；
- `setSampleIntervalForCurves(两条系数, 3)` 返回 2，这两条变成 **27** 点，第三条仍 79；
- `setCurves()` 重画后间隔仍然生效（17 点不变）——间隔由视图记住而不是只留在曲线对象上；
- 只带 `hasPointSize` 的补丁不碰颜色：作用域内 `pointSize` 变 11、作用域外仍是 5.0 且颜色不变，并直接断言 `!patch.hasColor`；
- 组级颜色补丁返回 4，组内四条全变、系数组不受影响；
- `setCurveLabel()` 设的显式标题在随后的 `applyStyle(Curves, …)` 中存活（不会被解析出来的继承标题覆盖）；
- `EditScope::All` + `hasVisible=false` 返回 7、`visibleCurveCount()==0`；重新显示后间隔仍是 2（**隐藏的曲线没有活的 item，间隔只能从视图记住的值读回**）。

**踩到的测试自身错误**：① 首版补丁忘了把 `hasVisible` 置 true，两条断言 FAIL；② 想直接读隐藏曲线的 `sampleInterval` 读不到（无活 item），改成「重新显示后再断言」；③ 删掉一条写成 `|| true` 的恒真断言。
### 9. `modules/visualization/tests/CMakeLists.txt`（29 行）

两个 `add_test`：`monitor_log_reader`（无界面）；`monitor_chart_render`（仅当 charts 目标存在，环境设 `QT_QPA_PLATFORM=offscreen`）。夹具目录从本文件路径推导，使 `modules/visualization` 独立配置时也能跑。

**收尾复核时改过一处**：渲染测试的出图目录 `AEROSIM_MONITOR_RENDER_DIR` 原默认值写作 `${CMAKE_CURRENT_LIST_DIR}/../../../build-pp/shots`，即**默认往源码树里写文件**。改为 `${CMAKE_CURRENT_BINARY_DIR}/monitor_render`。本机跑手工脚本时用 `-D` 覆盖，所以此前没暴露。

## 五、新增：夹具与夹具校验

### 10. `tests/fixtures/postprocess/su2_history_steady.csv`（80 行 = 1 表头 + 79 数据行，17 列）

**对应**：规划 §3.1 数据源选型、§3.2 许可与合规、P0 夹具要求。

来源 `https://github.com/alfiyandyhr/transonic-airfoil-optim` 的 `Solutions/baseline/history.csv`，2026-10-28 取得，原始 326 行数据 + 1 行表头（99,408 字节）。该仓库无 LICENSE（spdx NONE），因此按规划 §3.2 **不整包提交**，只裁剪首尾共 79 行。

**裁剪副作用（必须知道）**：前 40 行是原始第 0..39 步，随后跳到 287..325 步，因此 `Inner_Iter` 在 x≈40 与 x≈285 处有断档。看图时容易误认为绘制缺陷，实际是夹具裁剪造成的。

### 11. `tests/fixtures/postprocess/VerifyFixtures.cmake`（80 行 → 112 行）

**对应**：规划 P0「把 SHA256 加入 `_fixtures` 列表」。

- `_fixtures` 追加 `"su2_history_steady.csv|63191e605506aec7d391cbcc0856d0a28a1a8332b051dd742ad7758c7d4a38c4"`。
- 追加结构断言：80 行、17 列（16 个逗号）、列头含 `Time_Iter/Outer_Iter/Inner_Iter/rms[Rho]/CD/CL/CEff`、末行 `Inner_Iter` 推进到 325。
- 末行断言第一版写成 `MATCHES ",0,325,"` 而失败——该文件数值列是**空格填充的定宽字段**（`          0,           0,         325,...`），改为 `MATCHES ", *0, *325,"`。这个坑规划 §3.1 已提示过，我仍然踩了一次。
- 输出行从 `7 files ...` 改为 `8 files, ... 80 monitor lines (79 data rows)`。

## 六、文档

### 12. `docs/PostProcessTestData.md`（修改）

**对应**：规划 §10 交付物第 7 项。

- 文件数「七个」→「八个」；表格新增 `su2_history_steady.csv` 一行（内容 / 可检查结果）。
- 新增「`su2_history_steady.csv` 的来源与裁剪」小节：来源 URL、取得日期、原始规模、裁剪方式、断档说明、用途声明、无 LICENSE 的处理依据。
- `VerifyFixtures.cmake` 的实测输出更新为 8 files 版本。

### 13. `docs/PostProcessMonitorSeriesPlan.md`（恢复）

上游硬重置时该文件随工作区丢失。从 `backup/pre-reset-monitor-series` tag 恢复，内容与规划稿一致，**未修改**。

### 14. `docs/README.md`（修改，超出规划）

「后处理接口和功能规范」表新增两行：规划文档一行、执行汇总/修改日志一行。原因是 `docs/README.md` 是仓库文档导航的唯一入口，新增两份交付文档而不登记会使其无法被检索到。若认为这超出范围，可直接 `git checkout docs/README.md` 还原，不影响任何代码。

## 七、执行过程中修掉的真实缺陷（不是新增功能，是纠错）

| # | 缺陷 | 现象 | 根因 | 修法 |
| --- | --- | --- | --- | --- |
| 1 | 自检假通过 | `--selftest` 输出 `charts=0 curves=0` 却打印 `SELFTEST_OK`、返回 0 | 该分支只构造窗口就调 `selfTest()`，从不 `loadFile` | 强制要求路径参数，缺路径或加载失败都返回 1 |
| 2 | `visibleChartCount()` 恒为 0 | 自检输出 `visible=0`，与树里 14 条可见曲线矛盾 | 用 `chart->isVisible()` 统计，而窗口未 `show()` 时所有子控件都不可见 | 改用自维护的 `QSet<QString> visibleGroups_` |
| 3 | 切换叠加/分图丢失选中 | 切换布局后当前曲线消失 | 重建会清空 `currentId_`，随后又用它去取树项 | 重建前存 `keep`，重建后 `currentId_.clear(); selectCurve(keep);` |
| 4 | 顶部列数误导 | 显示「14 列」，与文件真实 17 列不符 | 该数字其实是曲线列数 | 文案改为「14 条曲线列」 |
| 5 | 实时追加静默失效 | 曲线不增长，也不报任何错 | 实时源读表头只做了 `trimmed()`，没剥求解器写在列名外的双引号；同一列在实时路径里叫 `"rms[Rho]"`、离线路径里叫 `rms[Rho]`，`curveId` 对不上，`appendPoints()` 找不到曲线返回 false | 把离线路径的引号剥离提为公有 `MonitorLogReader::trimmedField()` 供两条路径共用，并加 `testLiveAndOfflineAgreeOnColumnKeys()` 固定 |
| 6 | 实时重建后图表全部失联 | `chartFor()` 一直返回空 | 累加器产出的 `MonitorLog` 没带 `datasetId`，曲线 id 变成 `|CD` | `MonitorLogAccumulator::setDatasetId()`，且该 id **跨 `reset()` 保留**（同一求解器重启仍是同一份数据） |
| 7 | 首轮轮询把已有行误报成新增 | 自检 `first=20` 里混进了离线加载的 40 行 | 基准取 `liveAccumulator_.rowCount()`，而开始跟随时累加器是空的 | 改用 `model_.log().iterationStep.size()` 作基准 |

第 3 项的自检断言：`STEP5_OVERLAY charts=1 selected=su2_history_steady|CD`。

第 5–7 项的自检输出：`STEP10_LIVE idle_appended=0 first=20 second=19 rows=40->60->79 builds=1->1->1 incremental=1`。**`builds=1->1->1` 是这一项的关键**：40→60→79 行全程没有重建图表，说明增量通道确实生效，而不只是数据进去了。

## 八、绘图库从 QtCharts 换到 Qwt

**这一节改变了规划 §8 的既有结论，需与执行汇总第 4 节一起读。**

规划 §8.1/§8.2 记的是「已装 QtCharts，许可待确认」。实际执行中经用户确认改用 Qwt 7.5.2（`czyt1988/QWT`），理由是 QtCharts 在 Qt 5.12 为 **GPLv3 或商业**双许可，而 Qwt 为 **LGPL 2.1 + 三条例外**（子类不构成衍生、静态链接不构成衍生、只需致谢一句），可闭源商用静态链接。

替换范围严格限制在 `MonitorChartView.cpp` 一个实现文件——这正是规划 §9 风险表预留的「薄封装」：`MonitorCurve`/`MonitorAxisSummary` 等公开结构不含任何绘图库类型，`MonitorChartView.h` 只有三个前置声明 `class QwtPlot; class QwtPlotCurve; class QwtPlotPicker;`。

**换库过程中踩到的真实障碍（供后续复用）**：

1. **parasite 寄生图路线不可用**：该库的「每侧多条轴」只能靠 `createParasitePlot()`，它内部必经 `setCanvas()`；本机「删除 plot 自带画布 → 销毁 plot」必触发 Windows Debug CRT 堆损坏断言（`HEAP CORRUPTION DETECTED: before Free block`），官方 `setCanvas(nullptr)` 同样触发。无头环境下断言弹模态框表现为「卡死」，极易误判为死循环。
2. **改用单图双轴**：`enableAxis(QwtAxis::YRight, true)` + `item->setAxes(QwtAxis::XBottom, QwtAxis::YRight)`，全程不碰 `setCanvas`，绕开该缺陷。代价是每侧只有一条轴，故有 `kMaxMagnitudeGroups = 2` 的上限。
3. **`qwt_global.h` 在 `src/` 根**，不在 `src/core`，漏加 `-I` 会导致 153 个源文件全部编译失败。
4. **MSVC 19.51 移除 `stdext`**：Qt 5.12 的 `qcompilerdetection.h` 无条件展开 `stdext::make_checked_array_iterator`，需前置 include `<QtCore/qglobal.h>` 后再 `#undef`/`#define` 覆盖，否则覆盖无效。
5. **`qwt_plot_zoomer.h` 已改名为 `qwt_plot_canvas_zoomer.h`**。
6. **`QwtScaleWidget` 没有 `setTitleColor`**，改轴标题颜色要用 `QwtText::setColor` + `QwtPlot::setAxisTitle(QwtAxisId, const QwtText&)`。

## 九、新增：独立 Demo（超出规划）

### 15. `modules/visualization/demo/`（4 个文件，`MonitorChartDemoWindow.h` 234 行 / `.cpp` 1763 行）

底部样式面板的**拟合**一组控件：`fitCombo_`（不拟合 / 直线拟合 / 多项式拟合 / 移动平均）、`fitDegreeSpin_`（1..`MonitorCurveFit::maxDegree()`）、`fitWindowSpin_`（3..61，步进 2）、`fitLineStyleCombo_`（虚线 / 点线 / 实线）。与「连接方式」下拉并列但语义独立——这是用户指出语义错位后的修正。

- `MonitorChartDemoWindow.h`（234 行）/ `.cpp`（1763 行）：左侧树 + 右侧多图滚动区 + 底部样式面板 + 顶部工具条。
- `main.cpp`（71 行）：入口，支持 `aerosim_monitor_demo [history.csv]`、`--selftest`、`--shots=<dir>`。
- `CMakeLists.txt`（19 行）：`option(AEROSIM_BUILD_MONITOR_DEMO ... ON)`，只链 `aerosim_monitor_charts` 与 `Qt5::Widgets`，**不直接用任何 Qwt 头**——它同时是「公有 API 能从外部使用」的最小证明。

关键决策：

| 决策 | 原因 |
| --- | --- |
| **每个量级一张图**（`constexpr double kMagnitudeRatio = 10.0;` 贪心分桶） | 一张图两条轴最多容纳两个量级，而该夹具有三个量级（残差 -8..-1.6、小系数 0..0.9、CEff 4.9..83）。叠加时 CEff 会把其余 9 条压成贴地直线，连被调样式的 CD 都看不见 |
| 保留「叠加显示」开关 | 作为「为什么必须分图」的对照，让压平现象可复现 |
| `applyCurves()` 在 `setCurves()` 之后**必须逐条回写样式** | `setCurves()` 会清空 `explicitLabels_` 与已设样式 |
| `rebuildCharts()` 用 `takeAt(0)` + `hide()` + `deleteLater()` | 重建可能由这些控件自身的信号触发，不能 `delete` |
| 图内标题与左侧树分组标题都写**物理量名**（`残差`、`系数：CL · CMz · CFy`），不再出现 `系数（6 条同量级）` 这类计数式标题；标题改由 `MonitorChartView::setChartTitle()` 承载，图表上方的独立粗体标题行已删除 | 标题应当说明画的是什么；「几条曲线」在状态栏与图例里已有，重复陈述挤占了主题。计数式标题还把分组策略（分桶阈值 10.0）泄漏成了用户可见文字 |
| 左侧树列宽 `setColumnWidth(0, 300)` + `splitter->setSizes(330, 900)` + 分组项 tooltip | 物理量名列表较长，默认宽度会省略成 `系数：CD · CSF · CMx · CM…`；tooltip 兜住剩余情况 |
| 「跟随实时文件」按钮 + 500 ms `QTimer` → `pollLiveOnce()` | 演示实时链路；轮询是端口约定的形态（`MonitorLiveSource::poll()`），demo 只负责给它一个节拍 |
| 实时投递的**结构变化**（列集合变了 / 求解器重启 / 当前没有曲线）才重建，其余一律 `appendPoints()` 增量上屏 | 重建会丢掉缩放、选中与刚改的样式；而求解器每迭代一行都付不起这个代价 |
| `applyLiveUpdate()` 的基准行数取 `model_.log().iterationStep.size()` 而不是 `liveAccumulator_.rowCount()` | 开始跟随时累加器是空的，而窗口可能已经在显示从磁盘读到的行；用累加器算会把那批行误报成「刚追加」 |
| `loadFile()` 开头若正在跟随则先 `stopFollowing()` | 否则两个数据源会同时往一个模型里喂数据 |

## 十、未改动（核实项）

- `contracts/` 零改动。规划 §3.3 明确「不要在本任务里改 `contracts/`」。
- `modules/ui/`、`app/` 零改动。`grep MonitorChartView|MonitorLogReader|MonitorSeriesModel` 在两个目录命中 0 行。
- `modules/visualization/` 下原有的 `scene/`、`features/` 未动。
- 顶层 `CMakeLists.txt` 零改动：`AEROSIM_BUILD_VISUALIZATION` 默认 OFF，主程序不会因本次改动而构建本模块。

## 十一、构建与验证方式（本机限制）

本机 `cmake` **无法派生子进程**：`-G "Visual Studio 17 2022"` 报找不到 VS 实例（本机是 VS 18），`-G Ninja` 捕获到 `RC=139`。因此全部验证走 `build-pp/manual/` 下的手工 `cl.exe`/`link.exe` 链，编译与运行的是**同一份源码**，但**构建系统本身未经验证**。

可复现命令：

```bash
bash build-pp/manual/build_monitor_io.sh             # IO + 分析层与 monitor_test
bash build-pp/manual/build_monitor_demo.sh          # demo
bash build-pp/manual/build_chart_test.sh            # 曲线渲染测试
bash build-pp/manual/run_all_tests.sh               # 全量回归
MSYS2_ARG_CONV_EXCL='*' cmd /c 'build-pp\manual\run_monitor_demo_selftest.bat'
cmake -P tests/fixtures/postprocess/VerifyFixtures.cmake
```

运行环境（缺任一项都会静默失败或崩溃，容易误判为代码问题）：`PATH` 含 Qt bin、`QT_QPA_PLATFORM=offscreen`（渲染测试）、`QT_PLUGIN_PATH`、`TMP/TEMP` 指向可写目录。

出图必须用**原生 Windows 平台**而非 offscreen：渲染器是纯绘制路径不进事件循环，原生平台文字正常；offscreen 下无字体目录，只能看到线条、文字全空。