# 配置、工况与数值处理

本模块以公共契约为边界，不依赖 QWidget 或主窗口。当前目标 `aerosim_configuration` 使用 Qt Core / Xml，已实现控制方程、计算控制和输出控制明确参数的 XCF 读写；工况管理、任务快照和其他页面持久化尚未接入。

## 输出控制 XCF 读写

使用 `ParameterCategoryConfig` 的 `OutputControl` 分类：三个间隔为整数，两组输出变量与各边界历史选择为布尔 `ParameterEntry`。`OutputControlConfig` 维护固定的两组 17 项顺序与编码；UI 的 `OutputControlBinding` 在复选框名称列表与类型化参数之间转换。

- 三个间隔读写 `RefenceParametersData/CONTINUE_INTERVAL`、`WALL_INTERVAL`、`FLOW_INTERVAL`，后两项接受负数，直接保存，不推断负数含义。
- `OutputSettingData/VOLUME`、`BOUNDARY` 各为恰好 17 个分号分隔的 0/1。变量顺序与映射表一致；未知编码或数量不符拒绝加载，避免按错误位置覆盖原值。
- `OUTPUTBC` 用分号连接选中的边界名称，无选中项写空节点。候选边界来自原工程 `BCDataManager/BC_DATA/_ssName` 和已保存的 `OUTPUTBC` 名称，不再硬编码 M6 的边界。无工程边界时不虚构选项。
- `RESULT_INTERVAL`、`RESTART_OUT`、`PLOT_FUNCTIONS`、`ISFORCE`、`ISWALL`、`ISRESULT` 等未映射字段保留。未确认的开关不参与联动。
- 界面修改即时更新模型及未保存标记；重置本页恢复默认参数并清空边界选择，保留可选边界名称。欢迎页保存、另存为、退出保存覆盖输出控制。

测试逐一验证 17 个流场与壁面标记的位置、负数间隔、空／非空边界列表、非 M6 边界、错误编码及未映射字段保留，并通过界面修改后保存重开验证回填。

## 计算控制 XCF 读写

复用已有 `SolverConfigPayload`（`solverId=xcfd`、`schemaId=xcfd.calculation-control`、`schemaVersion=1`）维护七项参数。公共 `ParameterCategory` 尚无计算控制类别，因此不新增公共枚举，也不将其混入其他类别。`CalculationControlConfig` 负责字段校验和 XML 映射；`CalculationControlBinding` 负责 UI 草稿与数据结构转换。

读取／写回 `FOIS`、`NITSCG`、`dABS_RES`、`rms_limit_upper`、`ISGPU`、`GPUNUM` 和 `Geom_Type_Dat/_nProcess`。前六项位于 `RegionBlkMG/BasicRegion/_reg3`。GPU 关闭时仍保留卡数。未确认的 `GPUIndex`、`STARTNUM`、`_nMPI` 以及 `dREL_RES`、`ISABS_RES`、`NITSFG` 均保留源 XML 内容；指定 GPU 输入框注明暂不参与 XCF 读写。

打开文件时同步到计算控制草稿，编辑后立即同步到类型化模型，清除本页草稿恢复默认模型，欢迎页保存与退出保存检查覆盖这两页。实数采用 17 位精度和紧凑显示，避免读入后被原先 6 位小数控件截断。缺失字段使用默认值并记日志，重复字段、非法数值及不支持的开关编码拒绝打开且不替换当前工程。

## 控制方程 XCF 读写

数据流：`界面控件 ↔ UI 草稿适配 ↔ ParameterCategoryConfig ↔ XcfProjectDocument ↔ XCF`。

- 复用 `contracts/ConfigurationModels.h` 的 `ParameterCategoryConfig`、`ParameterEntry`、`ParameterValue`，以明确的枚举、布尔、整数、实数维护当前可编辑数据。不伪造 State、版本或不可变的 `ParameterSnapshot`。
- `XcfProjectDocument` 负责参数检查、XML 编码转换与文件读写；UI 的 `GoverningEquationsBinding.h` 负责 `QVariantMap` 与上述类型转换。界面编辑后立即更新数据结构，保存从数据结构取值，不遍历界面控件。
- `EquationSettingData/TURB_MODEL` 表示流态（层流 0、湍流 1、欧拉 2）；`TurbModelData/TURB_MODEL` 表示详细湍流模型（0～5）。对应关系见 [映射表](../../docs/XcfParameterMapping.md)。
- 读取先解析并检查临时文档，成功后替换当前状态。未知的有效选项编码、非法数值、重复映射节点或方程／流态冲突会拒绝打开；缺失字段使用默认值并给出日志。未启用的未知详细湍流模型保留原 XML 编码并提示，启用湍流后采用界面选择。
- 保存克隆完整 XML，只更新已映射字段；其他节点、属性、注释和未映射参数保留。格式缩进可能变化。使用 `QSaveFile` 原子写入，写入失败不替换源文件。
- 新建生成仅含已实现参数的 XCF，不代表完整求解输入。打开现有工程时其他页面尚不加载，其他页面的编辑尚不写回；它们在原文件中的内容保留。

文件入口：欢迎页 → 打开 / 保存工作区 / 另存为。工作台通过“返回欢迎页”进入上述页面；打开成功进入工作台，取消或失败留在欢迎页。保存页显示当前路径及未保存状态。控制方程修改会标记未保存，退出、新建或切换工程前提示保存。

测试：`xcf_project_roundtrip` 检查编码、数值往返、未知内容保留与失败路径；`xcf_project_ui` 检查文件读取、界面回填、编辑同步、保存和重置。

独立构建：`cmake -S modules/configuration -B <build-dir> -DCMAKE_PREFIX_PATH=<Qt5-root>`，再构建并运行 CTest。保持仓库 C++14 / Qt 5.12.12 / v141 x64 Debug 基线。

## 求解方法 XCF 读写

`SolutionMethodConfig` 将求解方法保存在 `ParameterCategory::SolutionMethod`，CFL 行使用 `cflPoints/<行号>/iteration` 与 `cflPoints/<行号>/cfl` 类型化条目；UI 通过 `SolutionMethodBinding.h` 转换草稿。只保存当前点数的行，界面缩减再增加点数仍保留临时编辑值。

`SPADIS`、`FLOWLIM` 按映射表编码 0～4，`TIMEDIS` 为 0～5；熵修正及强保稳为 0/1。定常时的时间参数、非 Roe 时的熵修正继续保留并读写。`TURBCFL_VAL`、`TurblstLn2` 及其他未知 XML 保留。

缺失字段采用默认值并记日志；重复节点、非法编码、非有限数值、CFL 点数与两列长度不一致均拒绝加载，当前工程不变。保存、重置及未保存状态沿用现有工程链路。`xcf_solution_method` 和 `xcf_project_ui` 覆盖文件及界面往返。
