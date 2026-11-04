# 主界面

`aerosim_ui` 是独立的 C++14 / Qt 5.12.12 / SARibbon 2.9.3 主窗口库，GUI 入口已从 `MockWindow` 切换到 `aerosim::MainWindow`。遵循《框架设计与技术调研.统一修改版》的五区布局。

应用默认先显示独立的 `StartupLandingPage`，采用蓝色竖向导航与浅色内容区，提供开始、新建、打开、保存工作区、保存模板、另存为和关闭工作区七个前端入口。最近工作区、内置模板和文件操作均明确标记为 Mock / 占位，不执行文件读写。双击最近项、点击“新建工作区”或打开页的 Mock 按钮后进入原有主工作台；主工作台“前处理 → 工作区 → 返回欢迎页”可重新打开入口页。`aerosim_gui --workbench` 可用于开发期直接启动主工作台，`--postprocess` 行为不变。

- 顶部采用 Word 风格三行 Ribbon，按顺序包含前处理、参数设置、计算、智能数据处理、后处理分析五个页签。大按钮和三行小按钮混排，每个命令使用内嵌 SVG 图标。
- 左侧始终显示一棵统一工作区树，根节点下同时包含五个模块。点击 Ribbon 标签只展开、选中并滚动定位对应模块，不重建树、不强制切换主显示区；WorkflowView 仍供运行门控使用，但不占用导航区。
- 前处理节点包含网格管理和弹道设置；参数设置节点包含七个不可继续展开的类别节点；计算节点整合工况、批处理计划、单次/批量计算和任务监控。智能数据处理与后处理分析当前只保留不可展开入口。点击具体节点时会反向选择所属 Ribbon 标签，并通过同步保护避免循环定位。
- 主显示区复用一个持久 VTK 视口插槽，并提供网格/弹道占位、工况、批处理计划和综合监控页面；仅切换 Ribbon 不重建视口或强制换页。
- 参数类别打开独立的前端配置弹窗；属性编辑器继续展示所选功能或对象信息。数值变化仍通过模块内 `parameterDraftChanged` 信号通知，尚未实现专业校验或文件持久化。
- 底部仅保留统一“输出”面板；日志、警告和错误按级别、模块、关键字与当前对象范围筛选。计算监控、收敛趋势和任务列表位于计算模块对应的主显示区。
- 面板支持停靠、浮动、关闭和界面工具恢复。状态栏明确标出未接入能力。

未实现的工程保存、网格导入、后处理和暂停/续算等操作禁用并提供提示；配置入口仅打开草稿。收敛曲线和 CPU/MPI 等指标不生成虚构数据。

## 接入边界

主窗口实现已有 `IWidgetHost`。Viewport、Log、Error、Monitor 各接受一次组件插入，成功后 Qt 父对象管理组件；失败不转移所有权。必须在 GUI 线程插入。原内置显示隐藏但保留，避免排队事件访问已释放控件。已插入组件若被销毁，可以重新插入。

`bindServices` 允许重复绑定和解除绑定，不重复连接。事件使用 `Qt::QueuedConnection`。`view()`、`requestRun()` 是同步契约，只在服务与窗口同线程时调用；服务内部应异步执行耗时任务。

`setRunContext` 接收服务提供的工况及参数快照，不从 UI 草稿构造已校验快照。当前 UI 仅识别 actionId 为 `run` 的运行操作（模块接入约定，不修改公共契约）；服务未提供此操作时保持禁用。不同工况及旧版本视图不覆盖当前已绑定工况。步骤、操作、问题可激活并发出 `navigationRequested(pageId, objectId, fieldKey)`；专业模块负责具体字段定位。

## 构建

GUI 现在必须显式提供 SARibbon 2.9.3 源码，无自动下载。库来源为 https://github.com/czyt1988/SARibbon ，MIT 许可证；沿用已批准的 2.9.3 基线，静态编译且关闭 FramelessHelper。无需 VTK 即可构建窗口。

Windows 主工程：

```bat
set "AEROSIM_QT_ROOT=C:\Qt\5.12.12\msvc2017_64"
set "AEROSIM_SARIBBON_SOURCE_DIR=<本机 SARibbon 2.9.3 源码目录>"
scripts\build-windows.cmd all Debug
build-windows\Debug\aerosim_gui.exe
```

独立模块（路径由本机传入）：

```sh
cmake -S modules/ui -B build-ui -DCMAKE_PREFIX_PATH=<Qt目录> -DSARIBBON_SOURCE_DIR=<SARibbon源码目录>
cmake --build build-ui --config Debug
cd build-ui
ctest -C Debug --output-on-failure
```

无界面构建仍使用 `AEROSIM_BUILD_GUI=OFF`，不依赖 SARibbon / Widgets / VTK。截图命令：`aerosim_gui --screenshot <输出PNG路径>`，保存窗口截图后退出。当前验证范围见 [主界面验证记录](../../docs/ui-validation.md)。

## 合并后的后处理工作台


`aerosim_ui` 提供后处理工作台 `PostProcessWorkbenchWindow`。窗口实现
`aerosim/ui/IntegrationPorts.h` 中的 `IWidgetHost`，由 `app/` 装配工作流和任务
服务；工作流规则、工程持久化、数据读取和 VTK 场景实现均不在本模块。

当前工作台包含：

- 顶部 Tecplot 风格的菜单与快捷工具栏：菜单按“数据、显示、特征、分析、工具”归类，工具栏保留结果导入/导出、网格/云图、切面/等值面/流线/轨迹、视图、探针与 DeepSeek Harness 等高频操作；
- 左侧“视图 / 数据 / 对象”标签页，依次管理当前 Frame 的全局图层、结果集/Zone/字段/时间帧，以及派生对象层级；
- 中央活动 Frame 视口插槽；未接入时显示明确标记的预览、Frame/字段/时间状态和方向提示，正式 Viewer 以 `WidgetSlot::Viewport` 注入；
- 右侧“属性 / Probe / 任务”检查器：属性随当前对象变化，Probe 只承载拾取结果，任务页呈现后续服务的进度与错误；
- 底部时间轴和可展开的消息面板。

工具栏当前只发送或记录操作请求，不伪造尚未实现的数据、特征或渲染行为。实际
VTK Viewer、统一场景入口和异步数据读取应分别由后续 `modules/visualization/scene`
与数据服务提供；UI 不直接持有 renderer、actor 或 VTK 数据对象。

`aerosim_postprocess_ui_smoke` 验证核心控件以及视口插槽只能接受一个 Viewer。

后处理窗口源码位于 `include/aerosim/ui/postprocess/` 与 `src/postprocess/`，测试位于
`tests/postprocess/`。开发期的 Mock 入口只用于切换 P0/P1 的会话初始状态；它明确不
承担数据读取、VTK 算法或场景管理，真实工程选择服务接入后将替换该入口。

默认启动 Ribbon 主窗口；本期“后处理分析”只保留导航入口，不从主窗口强制切换或打开业务页面。可运行 `aerosim_gui --postprocess` 直接打开已有独立后处理工作台。工作台的数据/场景面板、时间轴、属性面板及 DeepSeek Harness 入口均保留，主窗口不持有 VTK 数据。

真实 Viewer 将由后续 `modules/visualization/scene` 实现后经 `app/` 注入工作台；当前工作台始终使用明确标记的视口占位。`--screenshot` 可与 `--postprocess` 组合检查工作台。

## Ribbon 布局调整验证

见 [Ribbon 布局验证记录](../../docs/ribbon-layout-validation.md)。当前 CMake 实际锁定 SARibbon 2.9.2，与上文及开发约定中的 2.9.3 不一致；本次只调整界面，未修改依赖版本约束，版本统一需单独确认。

## 窗口皮肤

主窗口“后处理分析 → 外观 → 切换皮肤”提供 Office 蓝色、Office 浅色、深色、深灰色，调用 SARibbon 内置主题，同时调整停靠面板、属性编辑器、日志和视口占位的配色。Office 蓝色采用类似 Word 的蓝色标题栏与页签背景，当前页签为浅色，命令区保持浅色。选择立即生效，通过 `QSettings("AeroSimWorkbench", "Appearance")` 的 `theme` 键保存；下次启动恢复，未知值回退 Office 蓝色。不改变 Ribbon 分组或业务数据，独立后处理工作台暂保留原样。

本地兼容预览构建通过；6 项 CTest 通过，集成测试覆盖四种主题、重复切换、配置保存/恢复和无效主题回退。主题测试使用临时配置目录，不污染用户设置。可设置 `AEROSIM_THEME_SCREENSHOTS` 指向已有目录，让 `aerosim_ui_test` 导出各主题截图。本次按要求仅在本地修改，未在 Windows 目标机验证。
