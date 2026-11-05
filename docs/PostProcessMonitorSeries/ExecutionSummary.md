# 监控序列实现 — 执行汇总

- 规划文档：[docs/PostProcessMonitorSeriesPlan.md](../PostProcessMonitorSeriesPlan.md)
- 修改日志：[ChangeLog.md](ChangeLog.md)
- 分支：`postprocess/analysis-monitor-series`
- 基线：`1a56ca3`（上游 `origin/main`，本任务开始前工作区已硬重置到该提交）

## 1. 结论概述

规划 `P0` 全部完成并逐项验证通过；`P1`（接入后处理工作台 UI）**未做**；`P2` 中属于绘图库层的能力（叠加/双轴分组/横轴切换/对数降级）已在 `MonitorChartView` 实现并有回归断言，但**没有 UI 承载**；`P3` 按规划不做。

另有三项计划外产出，均已在修改日志中标「超出规划」：一是绘图库由 QtCharts 换成 Qwt 7.5.2（用户决定），二是独立的监控曲线 Demo 程序 `aerosim_monitor_demo`（用户要求），三是**实时读入链路**（`MonitorLiveSource` 端口 + `MonitorLogAccumulator` + `MonitorFileTailSource`）与绘图侧的增量通道 `appendPoints()`（用户要求）。实时部分的接口形态已定，但真正的数据来源形态仍未定，见待确认项 7。

演示过程中又按评审意见改了两处界面语义（均属本次新增代码，未触及既有接口）：图内标题由库内的统计摘要（`4 条序列 · 1 条纵轴`）改为由调用方用新增的 `setChartTitle()` 指定**物理量名**（`残差`、`系数：CL · CMz · CFy`），Demo 中重复的粗体标题行随之删除；分桶标题不再出现 `系数（6 条同量级）` 这类把分桶阈值泄漏成用户可见文字的形式。详见第 3.8 节。

**已知未验证项**：本机 `cmake` 无法派生子进程，全部编译验证走手工 `cl.exe`/`link.exe`，**构建系统（CMake/CTest 全链路）未经验证**。详见第 6 节。另有一处过程性教训值得记下：本次收尾复核时发现 `monitor_demo.exe` 的时间戳早于源码，自检跑到的是旧二进制（它仍打印了一行已从源码删除的 `STEP10_LIVE_DEBUG`）。杀掉进程重新编译后输出与文档一致。**GUI 结果与源码矛盾时，先确认二进制是不是最新，再怀疑代码** —— 见第 7.5 节。

## 2. 按规划条目核对完成情况

| 规划条目 | 状态 | 可验证结果 |
| --- | --- | --- |
| P0 · `MonitorLogReader`（`modules/visualization/data/`） | 完成 | `MonitorLogReader.cpp` 385 行 / `.h` 69 行；`monitor_log_reader` 测试 EXIT=0 |
| P0 · `MonitorSeriesModel`（`modules/visualization/analysis/`） | 完成 | `MonitorSeriesModel.cpp` 189 行；同一测试覆盖 `axisAvailable` 三态 |
| P0 · 两个 STATIC 目标 | 完成 | `aerosim_monitor_data`、`aerosim_monitor_analysis`，另加 `aerosim_monitor_charts` |
| P0 · 夹具 `su2_history_steady.csv` | 完成 | 80 行（1 表头 + 79 数据）、17 列、SHA256 `63191e60…4a38c4` |
| P0 · `VerifyFixtures.cmake` 加入 SHA256 | 完成 | 该脚本改成 112 行，实测输出 `8 files, … 80 monitor lines (79 data rows)` |
| P0 · 单元测试 | 完成 | `MonitorLogReaderTest.cpp` 698 行，全部通过 |
| P0 · 渲染测试 | 完成 | `MonitorChartRenderTest.cpp` 909 行，EXIT=0，并写出 PNG |
| P1 · 中央 `QStackedWidget` / 曲线视图 / 导入对话框 / 树数据驱动 | **未做** | 见第 5 节待确认项 1 |
| P2 · 叠加、双轴分组、横轴切换、退化置灰 | 绘图层完成，UI 未做 | `MonitorChartView` 有实现；`MonitorChartRenderTest` 断言 `axisCount()==2`、`pointSeriesCount()==2`、`legendLabels().size()==2` |
| P3 · CSV 导出 / 多文件对比 / Excel | 按规划不做 | — |
| §10 · `docs/PostProcessTestData.md` 更新 | 完成 | 文件数 七→八，新增来源与裁剪小节 |
| 超出规划 · 实时读入（用户新增要求） | 完成（来源待定） | `MonitorLiveSource` 端口 + `MonitorLogAccumulator` + `MonitorFileTailSource`，自检 `STEP10_LIVE rows=40->60->79 builds=1->1->1`；来源形态见待确认项 7 |
| 超出规划 · 绘图库换 Qwt 7.5.2（用户决定） | 完成 | `MonitorChartView.h` 458 行 / `.cpp` 1619 行，零 QtCharts 残留；见第 4 节 |
| 超出规划 · 独立 Demo `aerosim_monitor_demo`（用户要求） | 完成 | `MonitorChartDemoWindow.h` 234 行 / `.cpp` 1763 行，自检 10 步全绿 |

## 3. 关键实现说明

### 3.1 数据层不假设列集合

`MonitorLogReader` 按表头列名建索引，不硬编码列名。规划 §3.1 记录过另一份公开样例多出 `RefForce` 列，硬编码会直接崩。新增列只记 `warnings`，删除必需列才报错。

表头解析先 `trim` 再去成对双引号——该文件列名形如 `    "rms[Rho]"    `，引号内有对齐空格，不处理则 key 永远匹配不上。

### 3.2 迭代步的回退顺序是有依据的

`Outer_Iter` → `Inner_Iter` → 行序号。该 SU2 样例 `Time_Iter` 与 `Outer_Iter` **恒为 0**，真正推进的是 `Inner_Iter`（0→325）。只认 `Outer_Iter` 会画出一条完全水平的曲线，且不报错——这是最难排查的一类失败。

### 3.3 轴退化是显式判定，不是静默回退

`MonitorSeriesModel::axisAvailable(MonitorAxis, QString *reason)` 是唯一入口，三种原因文本分别对应「没导数据」「没有该列」「该列恒定」。对应规划 §5.3「不允许出现选了没反应」。

对数轴同理：`rms[Rho]` 全为负值（-1.60335846 → -8.008371149），请求对数时自动降级为线性，并在轴标题写「（线性轴）」、在状态栏点名受影响的曲线。

### 3.4 每条曲线是两个绘图对象

marks（`NoCurve` + 符号）与 joining（`Lines`，样条时挂 `QwtSplineCurveFitter`）分离。原因是规划 §5.3 要求「点为主粒度、连线可选」：`MonitorInterpolation::None` 时只画符号。图例只挂其中一个，避免图例出现重复条目。

### 3.5 改样式不重建图表

`setCurveStyle` / `setCurveLabel` 走 `applyLiveStyle()` 原地改，`chartBuildCount_` 不变（Demo 自检实测 `builds_before=21 builds_after=21`）。`setCurves()` 才重建。

选中态靠线宽与标记放大（3.5 / 1.6 倍）而非换色——颜色是调用方的编码信息（如按列序取色），选中态不应覆盖它。

### 3.6 每侧最多两组的硬上限

`constexpr int kMaxMagnitudeGroups = 2;` 超出者**整组不绘制**，并在状态栏列出被舍弃的分组名。

依据：该绘图库每侧只有一条纵轴（`QwtAxis::Position` 仅 YLeft/YRight/XBottom/XTop 四值），第三组只能并入右轴，而共享刻度正是分组机制要防的压平。所以宁可不画也不合并。回归断言：三组输入时 `axisCount()==2`、`pointSeriesCount()==2`、状态栏含 `group-c`。

### 3.7 `savePng` 的内缩不是随手加的

第一版用 `QwtPlotRenderer::renderTo()`，它把**整张图**当绘图矩形，装饰性文字紧贴边框被切掉——实测顶部标题丢了首字、右轴标题伸出右缘。改为内缩 10px 后手动调 `render(plot, painter, frame)`。

另注：渲染器在 plot 尺寸为 0 时直接 `return`，所以必须先 `resize()`。

### 3.8 图内标题的归属交给调用方

第一版的图内标题是库自己写的统计摘要：`%1 条序列 · %2 条纵轴`。评审时被否掉——**标题应当说明画的是什么物理量**（残差、系数），而不是复述有几条曲线；后者本来就在 `statusText()` 里。

改法（新增，未改动任何既有签名）：`MonitorChartView::setChartTitle(const QString &)` + `chartTitle()`。空串回落到内置摘要，因此老调用方一行不改行为不变。改标题只调 `plot->setTitle()`，**不重建 plot**，当前选区与缩放得以保留；`builtInTitle_` 缓存上一次 `setCurves()` 本会用的文字，清空调用方标题时无需重建即可恢复。

Demo 侧连带调整：删除图表上方的独立粗体标题行（与图内标题重复），分图标题改用分桶的物理量名 `系数：CL · CMz · CFy`；分桶标题不再出现 `系数（6 条同量级）` 这种把分桶策略泄漏成用户可见文字的形式（阈值 `kMagnitudeRatio = 10.0` 是 demo 层的实现细节）。左侧树分组标题同规则，并加宽列宽与 tooltip，避免长列表被省略成 `系数：CD · CSF · CMx · CM…`。

回归影响：`MonitorChartRenderTest` 对「条序列 / 条纵轴 / setTitle」零命中，无断言依赖旧标题。

### 3.9 拟合是独立能力，不是「连接方式」的别名

用户指出「连接方式」控件语义错位：它实际控制的是**相邻采样点怎么连**（不连 / 直线 / 样条），而收敛曲线真正需要的是**对整条趋势做拟合**。两者是正交的：

| | 连接（`MonitorInterpolation`） | 拟合（`MonitorFitStyle`） |
| --- | --- | --- |
| 语义 | 相邻两点之间画什么 | 整条序列的趋势是什么 |
| 是否穿过采样点 | **必须穿过每一个点** | 通常**一个都不穿过** |
| 数据来源 | 原始采样点 | 同一批采样点，重采样为 240 点趋势线 |

**Qwt 7.5.2 不含任何最小二乘拟合器**。`src/plot` 下只有 `qwt_curve_fitter` / `qwt_spline_curve_fitter` / `qwt_weeding_curve_fitter` 与 `qwt_spline*`，全部是**插值**（数学上必须过点）。因此拟合算法自己实现，并按分层约定放进 analysis 层（纯数学、不依赖绘图库）：

- 新增 `modules/visualization/analysis/MonitorCurveFit.h`/`.cpp`：`enum class MonitorFitMethod { None, Linear, Polynomial, MovingAverage }` + `MonitorCurveFit::fit(samples, method, degree, window)` 返回 `MonitorFitResult{points, rSquared, degree, window, ok, note}`。
- 多项式用**中心化 + 缩放到 [-1,1]** 后的法方程求解（高斯消元 + 部分主元，主元 < 1e-12 判奇异）。迭代步是数百量级，直接在原始 x 上求法方程精度损失过大。
- 移动平均用**居中窗口**，并在两端**收缩窗口**而不是丢点，保证趋势线覆盖首末采样点。
- 退化输入显式拒绝并给出中文 `note`（样本 < 3、所有 x 相同、参数非法），不画假趋势。

图表层（`MonitorChartView`，纯新增）：`MonitorCurve`/`MonitorCurveStyle` 各加 `MonitorFitStyle fit;`；新增 `fitSeriesCount()` 与 `fitResult(curveId)`；每条曲线的 `CurveItems` 增加第三层 `fitted`。四条设计约束：

1. **拟合默认虚线**（`Qt::DashLine`）。拟合是对数据的概括而非一次测量，不能与真实采样序列混淆。
2. **拟合不进图例**（`setItemAttribute(QwtPlotItem::Legend, false)`）。否则同一物理量会出现两个图例项，暗示测了两次。
3. **不外推**：只在采样 x 范围内重采样。
4. **用抽稀后的样本拟合**（`drawnSamples()`），因为拟合必须描述画布上真正画出来的东西。

改动拟合**方法/阶数/窗口**会重建该层（`rebuildFitSeriesFor`），因为 Qwt 把采样点存在 item 上；只改线型/颜色走 `applyLiveStyle`，不重建（`chartBuildCount()` 不变）。

新增单测 14 项断言（直线 R²=1 且不外推、噪声使 R² 落在 (0,1)、抛物线需 2 阶、阶数被夹到 n-1 并写 note、移动平均覆盖两端且削峰、乱序输入被排序、样本不足/垂直样本被拒绝、NaN 被跳过而非致命、`None` 不产生趋势）。

**已知局限（重要）**：多项式拟合在**采样稀疏的区间**会画出夹带区间里没有依据的曲线。该夹具由首 40 + 后 39 行裁成，x≈40 与 x≈285 之间有巨大断档，3 阶多项式跨过去会明显下凹。这是多项式拟合的固有性质，不是实现缺陷；接真实连续日志时不会出现。是否要在断档处打断拟合（或提示用户）列为待确认项。

### 3.10 采样控制是「间隔 + 作用范围」，不是「点数上限」

最初暴露给用户的控件是「每线采样上限」（一个点数上限）。它有两个问题：

1. **语义错位**：用户想表达的是「这图画得细一点还是粗一点」，即每 N 个点取 1 个；而「上限 5000」要求用户先知道这份文件有多少行。改名为**采样间隔**（`sampleInterval`，每 N 个采样点画 1 个）后，同一句话对 79 行的夹具和 10 万行的日志都成立。
2. **只能作用在整张图上**：它挂在 `MonitorChartView::setPointBudget()` 上，是视图级的一个数，无法表达「这三条曲线画稀一点」。

因此间隔改为**逐曲线**字段（`MonitorCurve::sampleInterval`，默认 1），并新增三种作用范围：

```cpp
enum class EditScope { All, Group, Curves };
int applyStyle(EditScope scope, const StylePatch &patch,
               const QString &groupKey = QString(),
               const QStringList &curveIds = QStringList());
```

`StylePatch` 的每个字段都成对出现（`hasPointSize` + `pointSize`），因为**组级编辑只能写用户真正动过的那一项**：若把所有字段一起写，「把这一组点调大」会顺手把整组刷成同一种颜色，而组内正是靠颜色区分成员的。

两类改动的生效路径不同，也是刻意的：

| 改动 | 路径 | 是否重建 |
| --- | --- | --- |
| 颜色、线型、点大小、点形、连线方式、拟合线型 | `applyLiveStyle()` | 否，`chartBuildCount()` 不变 |
| 可见性、采样间隔、拟合方法/阶数/窗口 | `setCurves()` 重建 | 是 |

原因是 Qwt 把采样点存在 item 上、把可见性烙进 item 在图例与轴上的存在：这两类东西变了，item 本身就得重建。间隔必须由视图记住（`sampleIntervalsById_`），因为**隐藏的曲线没有活的 item**，从图上读不回来。

全局 `setPointBudget()` 保留，但降级为「防病态文件的安全帽」——它和逐曲线的间隔取较大的那个步长，不再是 UI 暴露的主要手段。

### 3.11 实时读入：一个与来源无关的端口，加一条端到端参考实现

规划之外新增的一项能力：**一边求解一边看曲线**。原始需求里输入的形式还没定（可能是日志、可能是管道），所以接口按「与来源无关」设计，再附一条能跑通的参考实现。

三个新类型都在 `modules/visualization/data/`，关系是**端口 → 折叠 → 绘图**：

```cpp
// 端口：刻意不继承 QObject，纯拉取式
class MonitorLiveSource {
    virtual QString sourceId() const = 0;
    virtual bool start(QString *error) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual bool poll(MonitorUpdate *update, QString *error) = 0;
};
```

不继承 QObject 是刻意的：文件增长、子进程 stdout、socket、遥测总线都能挂在后面，而数据层不必知道事件循环怎么跑 —— 无界面环境里也能直接用。

`MonitorUpdate` 是**行增量**，不是整份快照：`columns` / `values`（[列][行]）/ `iterationStep` / `timeIndex` / `warnings` / `restarted` / `axisFromRowIndex`。行增量是这份接口唯一能以每次迭代的低成本更新的形态。

`MonitorLogAccumulator` 把增量折成一份 `MonitorLog`，**让 analysis 与 charts 两层完全不需要知道数据是分批到的**：它们看到的仍是离线读取那种完整日志。三个必须单独处理的边界情况：

| 情况 | 处理 | 为什么不能简单忽略 |
| --- | --- | --- |
| 求解器重启（文件被重写） | `restarted` 标记 → `reset()` 后继续追加，**不 return** | 重启标记与该轮首批行常在同一次投递到达，return 会丢掉那批行 |
| 运行中途新增列 | `widen()` 按已有行数回填 `NaN` | 填 0 会在图上画出求解器从未报过的平线；NaN 让 `points()` 自然跳过 |
| 文件没有迭代列 | `axisFromRowIndex` 续号（`step += startRow`） | 每轮从 0 重来会让横轴倒着走 |

`MonitorFileTailSource` 是参考实现：只读**追加的字节**（不是每次重读整份文件），把不完整的尾行留在 `pending_` 等到下次，文件变短即判定重写并报 `restarted`，`consumedBytes()` 暴露读位置供测试断言。

绘图侧新增一个**不重建**的通道：

```cpp
bool MonitorChartView::appendPoints(const QString &curveId, const QVector<QPointF> &points);
```

不用 `setCurves()` 重画，是因为重建会丢掉缩放、选中与用户刚改过的样式，而求解器每迭代一行都付不起这个代价。`appendPoints()` 把新点接到曲线的采样数组上，同步 `marks` / `joining` / `fitted` 三层，再按**整组**重算 Y 范围（轴是组共享的，只按单条曲线定范围会挤掉同组其它曲线），状态栏追加一句「已接收实时追加 N 行，未重建图表」。

**实现中修掉的一个真缺陷**：实时源读表头时只做了 `trimmed()`，没剥掉求解器写在每个列名外的双引号，于是同一列在实时路径里叫 `"rms[Rho]"`、离线路径里叫 `rms[Rho]`，两条路径的曲线 id 对不上，增量追加静默匹配不到任何曲线（表现为曲线不增长但也不报错）。修法是把离线路径的引号剥离提成公有方法 `MonitorLogReader::trimmedField()` 供两条路径共用，并补了一条断言「两条路径必须给出相同的列键」。

## 4. 与规划的偏离（需要复核）

### 4.1 绘图库由 QtCharts 改为 Qwt 7.5.2

规划 §8.1 记的是「已装 QtCharts，许可待确认」，§8.2 把依赖许可列为未确认项。执行中经用户确认改用 **Qwt 7.5.2（`czyt1988/QWT`）**。

理由是许可而非功能：QtCharts 在 Qt 5.12 为 **GPLv3 或商业**双许可，无法闭源商用；Qwt 为 **LGPL 2.1 + 三条例外**（子类不构成衍生作品、静态链接不构成衍生作品、只需一句致谢），可闭源静态链接商用。

受影响文件仅 `modules/visualization/charts/MonitorChartView.cpp`（实现）与 `MonitorChartRenderTest.cpp`（`CurveFactory` 签名）。`MonitorChartView.h` 只有三个前置声明，`MonitorCurve` / `MonitorAxisSummary` 等公开结构**不含任何绘图库类型**——这正是规划 §9 风险表预留的薄封装。

**能力差异（须知）**：QtCharts 支持任意多条纵轴，Qwt 每侧只有一条。因此在 >2 个量级的场景下，QtCharts 版能画而 Qwt 版不能。这是选型的实际代价，不是缺陷。

### 4.2 超出规划：独立 Demo

`modules/visualization/demo/`（4 个文件）。规划 P1 打算把曲线接进后处理工作台窗口，实际先做了一个独立可运行的程序 `aerosim_monitor_demo`，用于证明三层公有 API 能从外部使用。

**每个量级一张图**（`kMagnitudeRatio = 10.0` 贪心分桶）：该夹具有三个量级——残差 -8..-1.6、小系数 0..0.9、CEff 4.9..83。放一张图上时 CEff 会把其余 9 条压成贴地直线，连被调样式的 CD 都看不见。这与绘图库无关，是分组策略问题，QtCharts 版同样复现。

保留「叠加显示」开关作为对照，使压平现象可复现。

### 4.3 超出规划：`modules/visualization/CMakeLists.txt` 由骨架改写

原文件 8 行、`project(... LANGUAGES NONE)`、注释写「intentionally a skeleton」。改成 41 行的可用构建脚本。原注释的前提「等有可独立构建的实现和测试再加目标」在本任务已满足。

### 4.4 超出规划：`docs/README.md` 导航

「后处理接口和功能规范」表新增两行（规划文档、执行汇总/修改日志）。`docs/README.md` 是仓库文档检索的唯一入口，新增两份交付文档不登记就无法被找到。若认为超出范围，`git checkout docs/README.md` 可单独还原，不涉及任何代码。

## 5. 未完成 / 阻塞 / 待确认

### 待确认项 1：UI 接入（规划 P1，未做）

规划 P1 要求把曲线接进后处理工作台（中央 `QStackedWidget`、导入对话框、树数据驱动）。**当前完全未做**：`grep MonitorChartView|MonitorLogReader|MonitorSeriesModel` 在 `modules/ui` 与 `app` 命中 0。

未做的原因：上游把工作台窗口重写过（左侧三页 Tab + 中央视口 + 底部时间轴），曲线放哪一页、导入按钮接哪条信号，都属于需要用户定的产品决策。已核实的两条事实供决策：

- 上游 `openImportDialog` 的 `filesSelected` → `recordPendingImport` 只建「待接入的文件」组并 `emit importFilesRequested`，**该信号零接收者**，这是天然接线点。
- `contracts/include/aerosim/contracts/DataAccess.h` 的 `IResultAccess` 只有 `validateResult`，无读取入口。规划 §3.3 明确不在本任务改 `contracts/`，故未动。

### 待确认项 2：依赖许可（规划 §8.2）

规划 §8.2 把依赖许可列为未确认，本任务**保持现状未自行决定**。具体悬置的是：

- `build-pp/qwt7/` 下的 Qwt 7.5.2 源码与 17.5 MB 静态库**未入库**。
- `charts/CMakeLists.txt` 通过 `AEROSIM_QWT_ROOT` 缓存变量定位 Qwt，未写任何绝对路径（AGENTS.md 约束）。
- 是否把 Qwt 作为受控依赖入库、以何种形式（源码 / 预编译 / 子模块 / 外部安装），需许可与流程评审后决定。

### 待确认项 3：分桶阈值的位置

`kMagnitudeRatio = 10.0` 目前写死在 demo 内，未进库层。是否提升为 `MonitorChartView` 的可配置参数，待用户定。

### 待确认项 4：拟合如何对待采样断档

多项式拟合在采样稀疏区间会画出夹带区间里没有依据的曲线。当前夹具由「首 40 + 后 39」行裁成，x≈40 与 x≈285 之间是巨大断档，3 阶多项式跨过去会明显下凹（`demo_08_fit_Polynomial_4_g3.png` 可复现）。

这是多项式拟合的固有性质而非实现缺陷，真实连续日志不会出现。可选的处置方式有三种，**未自行决定**：

1. 保持现状（拟合如实反映数据，断档由使用者自己判断）。
2. 在 x 间隔超过某阈值处**打断**拟合，分成多段。
3. 检测到断档时**拒绝拟合**并在状态栏说明。

### 待确认项 5：`modules/visualization/spatial/` 缺失

上游硬重置时该目录与两个 `.vti` 夹具（`cube_t00_grid.vti`、`dense_grid_t00.vti`）一并丢失，导致 `spatial_test` EXIT=1。与本次改动无关。是否恢复待用户定。

### 阻塞项：git 提交与回退点标签

默认沙箱对 `.git` 只读，`git add/commit/tag` 报 `insufficient permission for adding an object to repository database .git/objects`。提升到 `danger-full-access` 的调用在本机会卡在审批弹窗直到超时，因此**本任务全程未做任何 git 写操作**。

当前实际状态：

- `HEAD = 1a56ca3`，**没有任何新提交**。所有改动都在工作区，未暂存。
- **回退点天然存在**：因为未提交，`git checkout -- .` + `git clean -fd` 即可回到 `1a56ca3` 的干净状态；该提交本身也在 `origin/main` 历史里。
- 计划中的便利标签 `baseline/monitor-series-start`（指向 `1a56ca3`）**未创建**。现有标签库里没有正好指向 `1a56ca3` 的标签：`rollback/pre-monitor-series-p2` 与 `pre-monitor-series-impl` 指向 `1a5f417`，`backup/pre-reset-monitor-series` 指向 `f16a8bd`（合并提交），`pre-rewrite-main` 指向 `ae29d72`。
- 提交时请**排除 `yihuo/`**（462 MB 的第三方二进制发行包，当前是未跟踪状态）与 `build-pp/`（已被 `.gitignore` 的 `build-*/` 覆盖）。是否把 `yihuo/` 加进 `.gitignore` 待用户定。

需要提交的文件（与修改日志逐条对应）：

```
M  docs/PostProcessTestData.md
M  docs/README.md
M  modules/visualization/CMakeLists.txt
M  tests/fixtures/postprocess/VerifyFixtures.cmake
A  docs/PostProcessMonitorSeriesPlan.md
A  docs/PostProcessMonitorSeries/ExecutionSummary.md
A  docs/PostProcessMonitorSeries/ChangeLog.md
A  modules/visualization/data/{CMakeLists.txt,MonitorLogReader.h,MonitorLogReader.cpp}
A  modules/visualization/analysis/{CMakeLists.txt,MonitorSeriesModel.h,MonitorSeriesModel.cpp,MonitorCurveFit.h,MonitorCurveFit.cpp}
A  modules/visualization/charts/{CMakeLists.txt,MonitorChartView.h,MonitorChartView.cpp}
A  modules/visualization/demo/{CMakeLists.txt,main.cpp,MonitorChartDemoWindow.h,MonitorChartDemoWindow.cpp}
A  modules/visualization/tests/{CMakeLists.txt,MonitorLogReaderTest.cpp,MonitorChartRenderTest.cpp}
A  tests/fixtures/postprocess/su2_history_steady.csv
```

### 待确认项 6：构建脚本的正确性未在真实 CMake 上验证

收尾复核时在 `CMakeLists.txt` 里发现并修掉了两个会直接导致构建失败的问题（详见修改日志第 5、9 项）：

1. `charts/CMakeLists.txt` 只把 `src/plot` 加进 include 路径，漏了 `src/` 根与 `src/core` —— `qwt_global.h` 与 `qwt_scale_map.h` 都找不到，必然 `C1083`。
2. `tests/CMakeLists.txt` 的渲染出图目录默认指向源码树内的 `build-pp/shots`，改为构建目录。

这两处都是**读代码发现的，不是跑构建发现的**——因为本机 CMake 不可用。它们说明一件事：手工 `cl/link` 链能证明「源码可编译可运行」，**不能证明「`CMakeLists.txt` 正确」**。后者必须在有可用 CMake 的环境上实际配置一次才算验证。

### 待确认项 7：实时源的来源与节奏

实时链路已按「与来源无关的端口 + 文件增长参考实现」做完（见 §3.11），但**真正的来源形态仍未定**（用户 m10110：「我目前也不清楚输入的类型，也可能是日志，先预留接口就好」）。悬置的问题：

- 求解器是把日志写文件（当前实现假设的形态）、还是通过 stdout 管道输出、还是走已有的 `MonitorSample` 遥测通道？前两种需要各自的 `MonitorLiveSource` 子类，第三种需要在多列日志与单指标流之间加一层映射。
- 轮询节拍：demo 用 500 ms，生产环境该由谁决定（UI 定 / 源自己推 / 求解器提供进度事件）？
- 求解器重启时曲线应当**清空重画**（当前行为）还是**保留并追加一个新 run**（需要给曲线加 run 分段）？
- 实时数据要不要落盘成历史文件，还是只在内存里累积（当前是后者）。

以上均未自行决定。当前实现的边界是：`MonitorLiveSource` 的四个纯虚函数就是预留接口的全部，来源定下来之后只需新增一个子类，charts 与 analysis 两层不用改。

### 待确认项 8：实时链路的验收只覆盖了「按文件增长」

`STEP10_LIVE` 用的是自检里临时造的文件（首 40 行 → 60 行 → 79 行三次重写），验证的是轮询、增量、不重建三件事。**没有验证**：真实求解器长时间运行时的内存增长、列中途出现的极端情形在高频投递下的表现、以及跟随期间用户同时改样式/缩放。这些需要真实的在线数据源才能有意义地测。

## 6. 验证结果

全量回归 `bash build-pp/manual/run_all_tests.sh`：

| 测试 | 结果 |
| --- | --- |
| `monitor_test`（reader + model + 拟合算法） | EXIT=0，输出 `Monitor log reader and series model OK` |
| `smoke`（工作台窗口） | EXIT=0 |
| `ui_test`（控件所有权） | EXIT=0，输出 `UI ownership... passed` |
| `chart_test`（曲线渲染） | EXIT=0，输出 `Monitor chart render OK`，并写出 5 张 1200x720 PNG + 19 张标记形态 480x300 PNG |
| demo 自检 | EXIT=0，`SELFTEST_OK`，含 `STEP7_MARKER_CONTENT shapes=Path,Pixmap,Graphic,Svg`、`STEP8_FIT fits=Linear(R2=0.056),Polynomial(R2=0.287),MovingAverage(R2=0.611)`、`STEP8_FIT_OFF series=0`、`STEP9_SCOPE_CURVES before=79 after=21 second=21`、`STEP10_LIVE idle_appended=0 first=20 second=19 rows=40->60->79 builds=1->1->1 incremental=1` |
| `VerifyFixtures.cmake` | 通过，`8 files, … 80 monitor lines (79 data rows)` |
| `spatial_test` | EXIT=1（缺夹具，见待确认项 5，与本次改动无关） |

**构建系统本身未验证**：本机 `cmake -G "Visual Studio 17 2022"` 报找不到 VS 实例（本机是 VS 18），`-G Ninja` 捕获 `RC=139`。全部验证走 `build-pp/manual/` 手工 `cl.exe`/`link.exe` 链，编译与运行的是同一份源码，但 `CMakeLists.txt` 与 CTest 注册**均未在可用的构建系统上跑过**。需在有权限环境复验。

## 7. 已知遗留缺陷与教训

### 7.1 Qwt 删画布即堆损坏

「删除 plot 自带的画布 → 销毁 plot」必触发 Windows Debug CRT 堆损坏断言（`HEAP CORRUPTION DETECTED: before Free block`），官方 `setCanvas(nullptr)` 同样触发。这直接否定了 parasite 寄生图多轴路线，改用单图双轴（`enableAxis(YRight)` + `item->setAxes(XBottom, YRight)`，全程不碰 `setCanvas`）。

**方法论教训（重要）**：Windows Debug CRT 断言默认弹模态框，无头环境下表现为「进程存活、无任何输出」，极易误判为死循环。任何「无输出 + 进程存活」都应先给探针加 `_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE)` + `_CrtSetReportFile(..., _CRTDBG_FILE_STDERR)`，否则会浪费大量时间在错误的假设上。

### 7.2 自检假通过

`--selftest` 分支曾只构造窗口就调 `selfTest()`，从不 `loadFile`，输出 `charts=0 curves=0` 却仍打印 `SELFTEST_OK` 并返回 0。已改为缺路径或加载失败均返回 1。

同一自检里 `visibleChartCount()` 曾用 `chart->isVisible()` 统计，窗口未 `show()` 时子控件全不可见故恒为 0，与树里 14 条可见曲线矛盾。已改为自维护 `QSet<QString> visibleGroups_`。

### 7.3 两条读取路径必须共用列名的归一化

实时源最初自己写了一遍「取表头字段」的逻辑（`field.trimmed()`），漏掉了求解器写在列名外的双引号，于是同一列在实时路径里叫 `"rms[Rho]"`、离线路径里叫 `rms[Rho]`。曲线 id 由 datasetId + 列键拼成，两者对不上，`appendPoints()` 找不到曲线就返回 false —— 表现为**曲线不增长，也没有任何报错**。

教训：同一份文件的两种读法必须共用同一段归一化代码，否则差异只在「两条路径汇合」的那一刻才暴露。已提成 `MonitorLogReader::trimmedField()` 并加断言固定。

### 7.4 断档是夹具造成的，不是绘制缺陷

`su2_history_steady.csv` 由原始 326 行裁成「首 40 + 后 39」，`Inner_Iter` 在 x≈40 与 x≈285 处跳变。看图时容易误判为绘制错误。

### 7.5 二进制过期会伪装成代码缺陷

收尾复核时观察到 demo 自检仍在打印 `STEP10_LIVE_DEBUG`，而该行早已从源码删除；查时间戳发现 `monitor_demo.exe`（09:19:02）早于 `MonitorChartDemoWindow.cpp`（09:19:25），说明那次跑的是旧二进制。杀掉进程重新编译后，输出与文档记录完全一致。

同类问题本次共踩到三次：一次是点形下拉框明明改成 20 项、截图里却仍是旧行为；一次是「每线采样上限」显示 20 而非默认的「全部」；一次就是上面这条。三次都出在**窗口进程还活着、`rm` 失败、链接失败但脚本照样打印 `LINK_DONE`**。

教训有两条：

1. GUI 结果与源码矛盾时，**先比对 exe 与源文件的修改时间**，再怀疑代码。
2. 构建脚本必须在 `rm` 与链接失败时**以非零码退出**，不能无条件打印成功。`build-pp/manual/build_monitor_demo.sh` 已改为 `rm -f ... || { echo LINK_FAIL; exit 1; }` 与 `LINK_RC` 检查。
## 8. 复现方式

```bash
bash build-pp/manual/build_monitor_io.sh       # 编译 IO + 分析层与 monitor_test
bash build-pp/manual/build_monitor_demo.sh     # 编译 demo
bash build-pp/manual/build_chart_test.sh       # 编译曲线渲染测试
bash build-pp/manual/run_all_tests.sh          # 全量回归

MSYS2_ARG_CONV_EXCL='*' cmd /c 'build-pp\manual\run_monitor_demo_selftest.bat'
MSYS2_ARG_CONV_EXCL='*' cmd /c 'build-pp\manual\monitor_demo.exe tests\fixtures\postprocess\su2_history_steady.csv'
```

运行环境缺任一项都会静默失败或崩溃，容易误判为代码问题：`PATH` 含 Qt bin、`QT_QPA_PLATFORM=offscreen`（渲染测试必需）、`QT_PLUGIN_PATH`、`TMP/TEMP` 指向可写目录。

出图必须用**原生 Windows 平台**而非 offscreen：渲染器是纯绘制路径、不进事件循环，原生平台文字正常；offscreen 下无字体目录，只能看到线条、文字全空。