# 单任务 / 多任务计算监控交付说明

基线：`main` 的 `d780adc`，日期：2026-11-05。
工作分支：`feature/compute-monitor-ui-wxd`。本次修改属于主界面的计算监控，不属于后处理工作台。

启动脚本使用独立构建目录 `build-monitor-main`，本机环境路径由未纳入版本控制的配置或环境变量提供。

## 本次负责的范围

邱的曲线组件通过独立适配器接入，任务选择与页面联动可单独验证。

| 内容 | 本次行为 |
| --- | --- |
| 单任务和批处理 | 只使用一个 `ComputeMonitorPage`，按任务 ID 切换；批处理子任务和单次任务使用同一详情区。 |
| 页面布局 | 使用 main 已有的左侧统一工作区树；中央共用监控页包含状态与进度、参数快照、曲线区，日志在曲线下方。区域可拖动调整大小。 |
| 批次节点 | 显示子任务数、运行/完成/失败数、成功完成比例和可点击子任务表；不把不同子任务的曲线、日志混在一起。 |
| 参数 | 显示选中任务的运行参数快照及来源标识，只读。修改运行配置、专业校验和重新提交任务属于配置/运行模块。 |
| 状态 | 展示公共契约中的未知、准备、运行、暂停、完成、失败、取消；“等待 / 准备中”对应 Preparing，没有自行新增公共状态机。 |
| 日志 | 每任务最近 2000 行；活动任务增量追加，后台任务只更新自己的缓存。切换任务恢复其历史。 |
| 曲线接入 | 提供可插入的 Widget 适配器，按任务替换历史或追加采样。缺少组件时显示明确提示，不生成虚构曲线。 |
| 数据隔离 | 用 taskId + attemptId 区分运行；有序事件拒绝重复/倒序，重试清空旧运行数据，过滤旧运行的迟到消息。 |
| 主界面 | “计算 → 任务监控 / 收敛曲线”和工作区树进入同一页面；保留 main 的欢迎页、五个 Ribbon 页签、配置弹窗、统一输出及后处理入口。 |
| 属性与输出 | 监控页内直接显示运行参数，进入时收起重复的属性停靠面板、折叠统一输出正文。离开监控页恢复此前状态；属性内容与输出筛选范围仍同步当前任务。 |

不包含：单次/批次创建向导、求解器调度、暂停/续算实现、参数持久化、日志解析、曲线绘图算法。现有启动按钮仍经工作流服务提交，不绕过校验，也不新增伪造的计算启动流程。

## 启动与演示

在本分支目录运行：

```powershell
# 正常入口：没有接入任务时显示空态
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/Run-ComputeMonitor.ps1

# 界面联动演示：明确标注为演示数据，不启动求解器
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/Run-ComputeMonitor.ps1 -Demo

# 已构建后仅运行
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/Run-ComputeMonitor.ps1 -NoBuild -Demo
```

本机路径从 Git 忽略的 `.vscode/monitor.local.json` 读取；其他电脑可直接提供
`AEROSIM_QT_ROOT`、`AEROSIM_SARIBBON_SOURCE_DIR`，或传 `-QtRoot` / `-RibbonRoot`。
脚本不删除文件、不结束正在运行的程序；重编译前自行关闭本目录正在运行的 GUI。
也可以通过主工程原有构建脚本构建 `aerosim_gui`，运行 `--monitor` 或 `--monitor-demo`。

演示包含两个独立任务和三个批次子任务，两个运行任务每秒分别产生演示进度、日志和采样，60 次后结束。
检查步骤：

1. 依次点击 Run_001、Run_002、Task_001、Task_002，观察高亮、参数、状态、日志和曲线区任务标识同时切换。
2. 停留在 Task_001；其他任务的演示更新不能覆盖当前详情。切回 Task_002 后能看到累计的日志。
3. 选择 BatchRun_001 查看统计，点击子任务表进入相同的详情区域。
4. 调整曲线/日志分隔条；点击“前处理 → 网格管理 → 网格文件”后恢复此前属性与输出面板状态；后处理入口仍可打开 main 的工作台。
5. 点击“计算 → 任务监控”重新进入；点击“计算监控”分组清空当前任务详情，检查上一任务曲线和日志不会残留。

演示仅在 `-Demo` / `--monitor-demo` 下生成任务。正常启动等待运行模块提供任务记录；main 其他模块原有的 Mock 工况及配置草稿保留，并不表示已有真实计算任务。动态演示约 60 秒后结束，重新启动演示即可重看。

直接运行 `build-monitor-main/Debug/aerosim_gui.exe` 会显示 main 原有欢迎页；`--workbench` 进入主窗口，`--monitor` 直接打开空监控页。应通过脚本配置 Qt 运行时后启动，或使用已有的项目开发环境。

## 与邱的组件衔接

适配器位于 `modules/ui/include/aerosim/ui/monitor/MonitorCurveWidget.h`，属于 UI 模块内部接口，不改 `contracts/`。
邱的 Widget 可以作为其子控件复用，只需实现两个操作：

| 方法 | 约定 |
| --- | --- |
| `replaceTask(taskId, attemptId, history)` | 先清除上一任务曲线，再显示新任务历史；空 ID 表示批次/空选择，必须清空。 |
| `appendSample(sample)` | 只会收到当前选中任务的增量采样，保留字段名、时间、单位与序号。 |

用 `window.monitorPage()->insertCurveWidget(adapter)` 插入一次，成功后 Qt 页面负责销毁。
重复插入被拒绝，失败时所有权仍属于调用方；已插入组件销毁后可重新插入。
切换任务不会反复创建 Widget。每任务最多缓存最近 5000 个采样，达到上限时将截断后的历史重新交给组件。
本次测试使用接收数据的替身组件确认切换/清空/追加行为，不宣称真实曲线渲染已经完成。

独立构建的页面提供任务树便于自测；嵌入主窗口时隐藏组件内部导航，使用 main 的统一工作区树。`taskChanged`、`selectionContextChanged`、`tasksCleared` 是 GUI 线程内的模块信号，用于同步导航与属性，不新增公共服务契约。批次表点击子任务会反向高亮工作区树。

## 数据提供方需要提供什么

复用既有 `TaskRecord`、`TaskEvent`、`MonitorSample`、`LogEntry`，不复制公共类型。

| UI 入口 | 数据提供方的责任 |
| --- | --- |
| `upsertTask(record, parameters)` | 提供 taskId、batchId、attemptId、工况、输入快照及显示用参数键值。用于目录初始化/元数据更新；不是每个进度事件都重新提交。参数映射是完整替换。 |
| `displayTaskEvent(event)` | 提供真实状态、进度和有序消息；序号按同一次运行递增。序号为 0 只适用于尚未进入有序流的消息。页面返回是否接受事件，主窗口据此过滤重复输出。 |
| `appendLog(entry, attemptId)` | 提供指定运行的日志。不要把已通过 TaskEvent.message 交付的同一日志再次交付。 |
| `appendMonitorSample(sample, attemptId)` | 提供指定运行的采样。相同 metric 的正序号递增；不同 metric 可使用各自序列。非有限数值被拒绝。 |
| `clearTasks()` | 工程/会话切换时清空目录、详情和曲线历史。 |

所有页面方法都在 GUI 线程调用。工作线程通过以页面为接收上下文的 `Qt::QueuedConnection` 投递，传值捕获消息和原始 attemptId；不要在回调执行时临时取“当前选中任务”作为消息归属。
监控采样和独立日志不含 attemptId 字段，因此用方法参数补充该信息；重试必须先交付新 TaskRecord。
缓存只用于当前窗口显示，不代替运行模块的持久化。

main 现有 ITaskService 仅提供 `record(taskId)` 和任务事件，没有任务目录枚举、参数值读取或采样/独立日志推送。
本次主窗口已连接现有任务事件，同线程时读取对应记录以识别批次；跨线程服务不进行同步查询，目录/参数由后续提供方排队交付。
解绑服务后，旧连接已排队的任务事件不会重新填充页面。参数真实值、历史任务目录及曲线数据源仍需要相应模块交付，不能把演示数据视作真实求解验收。

## 构建和验证

基线：C++14、Qt 5.12.12、MSVC v141 x64 Debug。监控组件只依赖 Qt Widgets 和现有 contracts，可以独立构建，不需要 VTK 或 SARibbon：

```powershell
cmake -S modules/ui/monitor -B build-monitor-module -G "Visual Studio 15 2017 Win64" -DCMAKE_PREFIX_PATH="$env:AEROSIM_QT_ROOT"
cmake --build build-monitor-module --config Debug
cd build-monitor-module
ctest -C Debug --output-on-failure
```

测试覆盖：真实鼠标选择、批次子任务入口、四区域一致性、后台任务更新隔离、事件去重/乱序、重试切换、旧运行迟到消息、曲线所有权和重新插入、历史上限、1000×600 与 1440×900 布局；主窗口测试还覆盖服务解绑后的排队消息和批次记录映射。

2026-11-05 本机实测结果：

| 验证 | 结果与记录 |
| --- | --- |
| 主工程 Debug 构建及 CTest | 10/10 通过，包括统一树联动、主界面集成、欢迎页、监控页和基线后处理检查；记录：`build-monitor-main/Testing/Temporary/LastTest.log`。 |
| 监控模块独立构建及 CTest | 1/1 通过；记录：`build-monitor-module/Testing/Temporary/LastTest.log`。 |
| Windows 原生窗口测试 | 默认缩放、150% 缩放均通过 3 个功能测试（QtTest 含初始化/清理共 5 项）；记录：`out/monitor-main-native-test.txt`、`out/monitor-main-dpi150-test.txt`。 |
| 布局检查 | 独立页检查 1000×600、1440×900；主窗口检查 1000×720、1440×960 及 150% 缩放。截图：`out/evidence/main/`，实际演示启动截图：`out/evidence/main/demo.png`。 |
| 提交前检查 | `git diff --check` 通过，启动脚本 PowerShell 语法检查通过；个人环境配置、构建产物及上述记录均被 Git 忽略。 |

上述构建产物、日志和截图为本地验证记录，不随源码提交；测试代码及复现步骤已包含在仓库中。

main 基线的 CMake 锁定 SARibbon 2.9.2，而根规范及本机基线为 2.9.3。本次版本检查同时接受 2.9.2/2.9.3，保留老师现有版本并用本机 2.9.3 验证；没有安装或替换用户环境。
其他 Qt 版本、Release、Linux 和老师电脑尚未验证。
