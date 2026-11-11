# 后处理「监控序列」端到端功能规划

状态：**规划稿，待评审**。本文件只描述要做什么、按什么顺序做、如何验收，不代表任何功能已实现。

负责人：好运的鸭（监控序列线）。对应 `docs/PostProcessTaskHandoff.md` 的 ⑫「系列线显示输出」相邻能力，即求解监控量的曲线显示与比较，不含多工况 Excel 汇总（③）与公式/舵效（⑪）。

## 1. 目标与验收定义

### 1.1 一句话目标

从**真实的 CFD 求解日志或 CSV** 导入，让后处理工作台生成监控序列树，并在中央工作区画出残值与升力/阻力/力矩曲线，支持曲线选择、叠加、以及「迭代步 / 时间」横轴切换。

### 1.2 什么算完成

按 `modules/visualization/AGENTS.md` 第 17 行原文：

> 一个功能只有在从现有后处理工作台的 UI 实际触发，并在 UI 中得到对应可见结果后才算完成。仅新增按钮、发出信号、写日志、改标签或单独运行算法均不算端到端完成。

因此验收必须是这条完整链路，缺任一环都不算完成：

```text
点「导入结果」 → 选真实文件 → 解析成功 → 左侧数据集树出现监控序列组与各序列节点
→ 选中序列 → 中央工作区出现二维曲线（真实数据点，非占位预览）
→ 勾选多条序列 → 同一坐标系叠加 → 切换「迭代步 / 时间」横轴 → 坐标轴与数据同步变化
```

同时必须保证（AGENTS.md 第 19 行）：已有工作台的打开、视口、数据/场景页、时间轴不被破坏。

## 2. 现状盘点（实测，2026-10-28）

### 2.1 已有的部分

| 位置 | 现状 |
| --- | --- |
| `modules/ui/src/postprocess/PostProcessWorkbenchWindow.cpp` | 后处理工作台窗口，44 KB，Ribbon + 三栏 + 时间轴布局完整 |
| 同上第 246-254 行 `createDataPanel()` | 数据集树 `postProcessDatasetTree`，**四个顶层组全部硬编码** |
| 同上第 336-339 行 | 右侧检查器已有「曲线与监控量」页（`InspectorCurve = 4`），但**只有一个横轴下拉框**，无复选框、无颜色、无叠加控制 |
| 同上第 149-206 行 `createActions()` | 16 个 Ribbon 动作，**全部只 `emit postProcessCommandRequested` + 写一行日志**，无真实业务 |
| 同上第 383-385 行 | `updateDataSelection` 已能把 type 为 `curve` 的节点切到曲线检查器 |
| `modules/visualization/` | **没有任何 CMake 目标**。`CMakeLists.txt` 只有 9 行骨架，`data/ analysis/ scene/ features/` 各只有 README.md |
| `tests/fixtures/postprocess/` | 7 个夹具，全部是 VTK 立方体与气动系数 CSV，**没有任何求解监控日志** |

### 2.2 三个必须正视的缺口

1. **没有真实监控数据样例**。`curves.csv` 是 α→Cz 的气动系数曲线（4 行），不是随迭代推进的监控序列。仓库里不存在残值/收敛历史文件。
2. **数据集树是假的**。第 250-253 行把「流场体网格」「压力 [Pa]」「t = 0.0 s」「∿ 阻力系数监控曲线」写成字符串字面量。要接真实数据，这棵树必须改成数据驱动。
3. **smoke 测试锁死了这棵假树的索引**。见 2.3，这是最容易被忽略的连带约束。

### 2.3 测试现状与其刚性约束

`modules/ui/tests/postprocess/PostProcessWorkbenchWindowSmoke.cpp`（61 行）第 37-45 行：

```cpp
tree->setCurrentItem(tree->topLevelItem(1)->child(0));   // 要求第 2 组第 1 项是 field
tree->setCurrentItem(tree->topLevelItem(2)->child(2));   // 要求第 3 组第 3 项是 time
tree->setCurrentItem(tree->topLevelItem(3)->child(0));   // 要求第 4 组第 1 项是 curve
```

一旦把硬编码树换成数据驱动，这三行就会把 `return 8 / 9 / 10` 打出来。**因此「改树」与「改 smoke 测试」是同一件事，必须同一个提交完成**，不能等 CI 红了再补。

改造方向：把按下标定位改成按 `Qt::UserRole` 的 type 与 objectName 定位，例如新增稳定角色值 `monitorRoot` / `monitorSeries`，用遍历查找代替 `topLevelItem(n)`。断言仍然保留「不同类别的选择切到不同检查器页」这个真正的契约。

### 2.4 中央工作区的插槽约束

`insertWidget(WidgetSlot::Viewport, QWidget*)` 第 394 行实现：**只接受一次**，第二次返回 false（smoke 测试第 53-58 行就断言了这个行为）。当前中央是 `ResultPreviewWidget` 占位预览（第 106-129 行，纯 QPainter 画的翼型，注释明确写着它 "is not a fake data source"）。

→ 已确认方案：**中央区改为 QStackedWidget，第 0 页是现有视口容器，第 1 页是曲线页**。这样既不动 `insertWidget` 契约（VTK 视口将来仍插到第 0 页），也不让曲线和视口抢同一个位置。

## 3. 数据源选型

### 3.1 选定格式与真实样例

选用 **SU2 的 `history.csv`**。理由：它是开源求解器公开导出、天然同时含**残差**与**升阻力/力矩**的单一 CSV，列名自描述，无需私有文档。

真实样例（2026-10-28 实测取得并解析）：

| 项 | 值 |
| --- | --- |
| 来源 | `https://github.com/alfiyandyhr/transonic-airfoil-optim` 的 `Solutions/baseline/history.csv` |
| 规模 | 326 行数据 + 1 行表头，99,408 字节 |
| 列数 | 17 |
| 列头（原文，引号内带空格填充） | `"Time_Iter","Outer_Iter","Inner_Iter","rms[Rho]","rms[RhoU]","rms[RhoV]","rms[RhoE]","CD","CL","CSF","CMx","CMy","CMz","CFx","CFy","CFz","CEff"` |
| 第一行数据 | `0,0,0,-1.60335846,0.774301042,0.9099733256,3.878948724,0.0803708097,0.3943160569,0,0,0,0.1224002601,0.06441212278,0.3972350667,0,4.906209834` |
| 末行 | `0,0,325,-8.008371149,-5.029542445,-6.059190702,-2.520610836,0.01100871693,0.9146069626,0,0,0,0.1315928978,-0.02586454576,0.91430745,0,83.08025067` |

**解析要点（都是实测踩到的，实现时必须处理）：**

- 列名带**双引号且引号内有对齐空格**（`    "rms[Rho]"    `），必须先 `trim` 再 `unquote`，否则 key 匹配不上。
- 数值列是**空格填充的定宽字段**（`       -1.60335846`），需要 trim 后转 double。
- 该样例 `Time_Iter` 恒为 0，`Outer_Iter` 也恒为 0，推进的是 `Inner_Iter`（0→325）。这是**定常算例**。非定常算例才会让 `Time_Iter` 前进。
- 另有一份公开样例 `nikitaaa27/AE462-git` 的 `Lab6/CFD/a=0/history.csv`（319 行）列头多一个 `"RefForce"`，说明**列集合会随求解器配置变化**，读取器不能假设固定 17 列，必须按列名索引。

### 3.2 许可与合规（必须写进依赖说明）

- 上述样例仓库**均无 LICENSE 声明**（已逐个查 `license.spdx_id`，返回 NONE）。
- → 因此**不整包提交第三方数据**。做法：裁剪为首尾各若干行的**小样本**（≤ 80 行）作为夹具，在 `docs/PostProcessTestData.md` 记录来源 URL、取得日期、裁剪方式与「仅用于格式解析验证」的用途声明。
- 该原则与 `docs/PostProcessTestData.md` 第 27 行既有规定一致：真实仿真数据只记录来源与期望，不提交到 Git；大型数据放受控位置。

### 3.3 与既有契约的关系（重要，避免重复造）

- `contracts/.../Telemetry.h` 已有 `MonitorSample{taskId, timestamp, sequence, metric, value, unit}`，`TaskModels.h` 有 `TaskEventType::ResidualUpdated` 与 `TaskProgress.residual`。
- 但那是**求解过程的在线遥测**（单值、跟活任务），本功能是**离线读取历史文件**（批量、多列）。两条路径，**不要混用**，也不要在本任务里改 `contracts/`（改契约需先过接口评审，见 `docs/PostProcessContracts.md`）。
- 本任务所需的标识一律复用现有轻量类型：数据集用 `DatasetRef{datasetId, fileId}`，不新增跨模块结构。

## 4. 架构落位

遵守 `modules/visualization/AGENTS.md` 的分层与依赖方向（`analysis` 不依赖 `scene` 或 QWidget）：

```text
modules/visualization/data/      MonitorLogReader：CSV 解析、列目录、单位推断（无 Qt Widget，无 VTK）
modules/visualization/analysis/  MonitorSeries 数据模型：序列、横轴候选、轴退化判定、叠加选择（无 QWidget）
modules/visualization/ui 侧      modules/ui/src/postprocess/ 内新增曲线视图控件（QWidget + QtCharts）
```

- **不引入 VTK**。本机无 VTK 安装，而监控曲线与三维场景无关，因此 `data/` 与 `analysis/` 两个目标**不链接 VTK**，可在 `AEROSIM_BUILD_VISUALIZATION=ON` 下正常构建（该开关当前只控制是否 `add_subdirectory`，不强制 VTK）。
- 曲线视图控件放 `modules/ui`，符合「Widget 渲染留在 GUI 线程」「analysis 不依赖 QWidget」的双向约束。

### 4.1 建议接口（草案，非冻结）

```cpp
// modules/visualization/data/MonitorLogReader.h
struct MonitorColumn { QString key; QString unit; bool isResidual = false; };

struct MonitorLog {
    QString datasetId;
    QString sourcePath;
    QVector<MonitorColumn> columns;              // 按列名索引，不假设顺序
    QVector<qint64> iterationStep;               // Outer_Iter 缺失时回退 Inner_Iter
    QVector<qint64> timeIndex;                   // Time_Iter
    QHash<QString, QVector<double>> values;      // key -> 列数据
    QStringList warnings;                        // 非致命问题（未知列、空列等）
};

class MonitorLogReader {
public:
    static bool canRead(const QString &path);                       // 后缀 + 首行列名探测
    static bool read(const QString &path, MonitorLog *out, QString *error);
};
```

```cpp
// modules/visualization/analysis/MonitorSeriesModel.h
enum class MonitorAxis { IterationStep, TimeIndex };

class MonitorSeriesModel {
public:
    bool setLog(const MonitorLog &log, QString *error);
    bool axisAvailable(MonitorAxis axis) const;   // 轴退化判定，见 5.3
    QVector<QPointF> points(const QString &columnKey, MonitorAxis axis) const;
    bool isResidual(const QString &columnKey) const;  // 残差建议对数轴
};
```

不确定的细节（列名别名、单位推断、多文件合并规则）留到实现时按最小样例验证，不在规划阶段冻结。

## 5. 端到端交互流程（UI 侧要做什么）

### 5.1 入口

复用既有「导入结果」动作（`postProcessImportAction`，第 192 行）。当前它只发信号并写「已打开导入结果流程」。改为：打开文件对话框（`QFileDialog`，过滤 `*.csv`），确认后走解析。文件对话框属「短暂交互」，符合 `modules/visualization/README.md` 对对话框的定位。

### 5.2 数据集树改造

保留四个顶层组的语义，但内容改为数据驱动：

| 组 | 数据源 | 说明 |
| --- | --- | --- |
| 网格与 Zone | 无真实数据时保留占位并标注 | 属空间线（陌辞辞）职责，本任务不接管 |
| 物理字段 | 同上 | 不接管 |
| **监控序列**（新增/替换现有「派生对象」中的曲线项） | **MonitorLog 的列目录** | 每个数值列一个节点，残差列加记号，单位显示在名字后 |
| 派生对象 | 保留 | 曲线不再挂在这里，避免语义混淆 |

节点 `Qt::UserRole` 增加稳定角色值（`monitorRoot`/`monitorSeries|columnKey`），供代码与测试按角色定位，不依赖下标。

### 5.3 曲线页与横轴

- 中央 `QStackedWidget` 第 1 页嵌入曲线控件，首条序列被选中时自动切换过去。
- 横轴下拉沿用右侧曲线检查器现有控件，选项收敛为两个真实可用的轴：**迭代步**、**时间**。
  - 若 `Time_Iter` 全为同一值（本例即如此）→ **时间轴退化**，该选项置灰并给出可读原因提示（例如「该文件为定常推进，时间索引恒为 0」）。不允许出现「选了没反应」。
  - 原下拉里的第三项「工况序号」在本功能范围内无数据支撑，移除或同样置灰说明。
- 曲线检查器补齐：序列列表 + 每序列可见性复选框 + 颜色 + 线型；残差列默认**对数 Y 轴**，升阻力默认线性 Y 轴，允许手动切换。
- **不复用底部时间轴滑块**。底部滑块是 VTK 结果帧的全局状态，监控曲线的迭代步是另一维度，复用会造成语义混乱（AGENTS.md 要求顺手修语义不一致）。

### 5.4 叠加

同一坐标系内多序列叠加；不同量纲（残差 ~1e-8 与 CL ~0.9）叠加需要双 Y 轴或归一化，**默认按量纲分组**：残差一组（对数轴），气动系数一组（线性轴）。这是「曲线选择/叠加」的核心可用性决定，必须显式呈现，而不是让两条量级差 8 个数量级的线挤在一张图里。

### 5.5 错误反馈

解析失败、列缺失、空文件、非 UTF-8 等，都要走 `appendMessage` + 状态栏 + 树内可见提示，**不允许静默失败**。

## 6. 分阶段实施计划

### P0 数据层（无 UI 依赖，可独立验收）

- 新增 `modules/visualization/data/MonitorLogReader.{h,cpp}` 与 `modules/visualization/analysis/MonitorSeriesModel.{h,cpp}`。
- `modules/visualization/CMakeLists.txt` 新增两个 STATIC 目标（不含 VTK、不含 Widget）。
- 夹具：`tests/fixtures/postprocess/su2_history_steady.csv`（裁剪 ≤ 80 行），并把 SHA256 加入 `tests/fixtures/postprocess/VerifyFixtures.cmake` 的 `_fixtures` 列表（该脚本已做 CRLF→LF 归一，第 26-28 行）。
- 单元测试 `MonitorLogReaderTests`：列数、指定列数值（首行 `rms[Rho]` = -1.60335846、末行 `CD` = 0.01100871693）、列名去空格去引号、未知列告警、缺列报错、空文件报错、轴退化判定。
- 验收：`ctest` 中该测试通过 + `postprocess_fixture_check` 通过。

### P1 端到端打通（单曲线）

- 中央改 QStackedWidget；新增曲线视图控件（`QChart` + `QChartView`）。
- 「导入结果」接文件对话框 → 解析 → 树填充 → 点序列 → 曲线出现。
- **同步改造 smoke 测试**（2.3），把下标断言换成角色断言。
- 验收：真实文件从 UI 导入后能看到真实曲线；`postprocess_ui_smoke` 与 `ui_window_integration` 均通过。

### P2 叠加与横轴

- 序列复选框、颜色/线型、叠加、双轴分组、横轴切换与轴退化提示。
- 验收：勾选 2+ 序列同屏叠加；切横轴后曲线与轴标签同步；退化轴置灰且有原因。

### P3 可选（不在本次必交范围）

- 曲线数据 CSV 导出、多文件对比、与 ③ Excel 汇总打通。

## 7. 测试与验收计划

| 层次 | 内容 | 环境 |
| --- | --- | --- |
| 单元 | MonitorLogReader / MonitorSeriesModel 数值与错误路径 | 本机可用（无需 VTK） |
| 夹具完整性 | `postprocess_fixture_check`（新夹具 SHA256 + 列头检查） | 本机可用 |
| UI smoke | 改造后的 `postprocess_ui_smoke`（角色定位、检查器切换、插槽唯一性） | 本机可用（offscreen） |
| 端到端 | 目标机手动：`scripts\run-postprocess-windows.cmd` → 导入真实文件 → 看图 | **目标机 F:\AeroSimWorkbench** |

本机已有可用链路（2026-10-28 实测）：`deps/configure.bat`（CMake 3.18.2 + Ninja + MSVC 14.29 + Qt 5.12.12 + SARibbon 2.9.2 静态）→ `deps/buildall.bat` → `deps/ctest.bat`，当前 6/6 通过。

**未验证项要如实写进 PR**：Windows VTK 8.2 环境下的完整后处理工作台未在本机跑过（本机无 VTK）；目标机验收需负责人执行。

## 8. 依赖变更

### 8.1 QtCharts（已安装并实测可用）

本机 `.tools/Qt/5.12.12/msvc2017_64` 初始**不含** QtCharts（`lib/cmake` 53 个模块中无 `Qt5Charts`，无 chart dll/lib）。已通过项目内 aqt 补装：

```bash
./.tools/venv/Scripts/aqt.exe install-qt windows desktop 5.12.12 win64_msvc2017_64 -m qtcharts --outputdir .tools/Qt
```

安装结果：`include/QtCharts`（112 个头文件，**无二进制损坏**）、`bin/Qt5Charts.dll`/`Qt5Chartsd.dll`、`lib/Qt5Charts.lib`/`Qt5Chartsd.lib`、`lib/cmake/Qt5Charts`。

编译与运行探针实测通过：`find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Charts)` 配置成功，构建 `BUILD_EXIT=0`，运行输出 `QT_CHARTS_OK points=10`。探针工程位于 `deps/probe-charts/`（属本地工具目录，不入库）。

### 8.2 ⚠️ 许可待确认（阻塞项，需老师拍板）

- QtCharts 在 Qt 5.12 中是 **GPLv3 或商业双许可**模块（非 LGPL）。
- 本仓库根目录**没有 LICENSE 文件**，也无任何许可声明。
- 引入后，含该模块的分发物即受 GPLv3 约束。当前为课程/内部协作、未对外分发，风险可控，但**必须在 PR 与文档中显式写明**。
- 备选：改用自绘 `QPainter` 图表控件（零许可风险，但对数轴/缩放/图例/导出需自行实现，工作量明显增加）。
- 需要确认的问题只有一个：**本项目的分发范围是否允许 GPLv3 依赖？**

### 8.3 不需要的依赖

VTK 不需要。`CMAKE_PREFIX_PATH` 与 `SARIBBON_SOURCE_DIR` 等既有配置不变。

## 9. 风险与对策

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| QtCharts GPLv3（8.2） | 可能需换方案、重写图表层 | 曲线视图与图表库之间留一层薄封装（`MonitorChartView` 接口），换库只改实现 |
| smoke 测试锁死假树下标（2.3） | 改树必然打破 CI | 同一提交完成树改造 + 测试改造 |
| 样例无 LICENSE（3.2） | 合规争议 | 只提交裁剪小样本 + 来源与用途声明 |
| `Time_Iter` 退化（3.1） | 「时间横轴」在本样例上无意义 | 轴退化显式置灰 + 原因提示（5.3） |
| 列集合随配置变化（3.1 RefForce） | 硬编码列名会崩 | 按列名索引，未知列仅告警不失败 |
| 本机无 VTK | 无法本机验证完整工作台 | 明确声明未验证项，目标机验收 |
| 文档冲突：AGENTS.md/DevelopmentEnvironment 写 SARibbon **2.9.3**，`cmake/SARibbonDependency.cmake` 校验写死 **2.9.2** | 目标机可能配置失败 | 本任务不改此冲突，但 PR 中标注；构建以 CMake 校验为准 |

## 10. 交付物清单

1. `modules/visualization/data/MonitorLogReader.{h,cpp}` + `modules/visualization/analysis/MonitorSeriesModel.{h,cpp}`
2. `modules/visualization/CMakeLists.txt` 两个新目标
3. `modules/visualization/tests/MonitorLogReaderTests.cpp`（或按现有 `tests/` 约定放置）
4. `tests/fixtures/postprocess/su2_history_steady.csv` + `VerifyFixtures.cmake` 更新
5. `modules/ui/src/postprocess/` 内曲线视图控件 + 工作台改造（导入、树、曲线页、叠加、横轴）
6. `modules/ui/tests/postprocess/PostProcessWorkbenchWindowSmoke.cpp` 改造
7. 文档更新：`docs/PostProcessTestData.md`（新夹具与来源）、`docs/PostProcessTaskHandoff.md`（⑫ 状态，并修正第 3 行关于 `aerosim_spatial_features` 的过期描述）
8. 分支 `postprocess/analysis-monitor-series`，从最新 `origin/main` 拉出

## 11. 附：可复现的样例取得命令

```bash
gh api "repos/alfiyandyhr/transonic-airfoil-optim/contents/Solutions/baseline/history.csv" \
  --jq '.content' | base64 -d > history.csv
wc -l history.csv   # 327（1 表头 + 326 数据）
head -1 history.csv # 17 列，见 3.1
```
