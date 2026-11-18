# ParaView 历史 CGNS 读取器验证

状态：本地技术验证，不是生产接入或完整格式验收。

## 验证范围和环境

- ParaView 5.6.3，提交 `587b66e78632f6d30443c990141c4a99dec74f0a`，选取 `ParaViewCore/VTKExtensions/CGNSReader` 中的 `vtkCGNSReader` 及其必需的 SIL、信息键等源码；该版本所引用的 VTK 子模块是 ParaView 的指定提交，不能假定与独立 VTK 8.2.0 完全相同。
- macOS Apple Silicon + Rosetta，使用本机 x86_64 VTK 8.2.0、CGNS 4.4.0 和 HDF5；为了实验在本机 VTK 安装中额外启用了 `vtkParallelCore` 和 `vtkpugixml`。此记录不代表 Windows VS2017 Debug 通过验证。
- 独立临时 CMake 程序在项目仓库之外编译，不改项目现有读取器或公共接口。ParaView 5.6.3 使用的 `cgio_read_all_data`、`cgio_read_data`、`cgio_read_block_data` 已不在本机 CGNS 4.4 的头文件中；本次临时包装先查询节点的数据类型，再转调相应 `*_type` API。包装仅用于验证，不是已审核的生产兼容层。
- 读取前先执行 `UpdateInformation()`，启用 Base、Family、节点／单元字段，再通过 `GetSIL()->Select("/Patches")` 选择边界。该版本的 `SetLoadBndPatch` 是已弃用的空实现，不能依靠它启用边界。

## 实测结果

| 输入 | 历史读取器结果 | 与现有适配器比较 |
| --- | --- | --- |
| M6 Ma5 流场 | 107973 点、257817 体单元，9 个节点字段；6 个边界片区 | 体和各片区单元数一致；逐个片区按实际被单元使用的节点统计，`p_dim` 范围与现有适配器输出一致。 |
| M6 Ma6 流场 | 107973 点、257817 体单元，9 个节点字段；6 个边界片区 | 体和各片区单元数一致；片区实际使用节点的 `p_dim` 范围一致。 |
| M6 网格文件 | 体网格和 6 个边界片区 | 当前适配器输出 5 个边界区域：它按 ZoneBC 合并了三角形和四边形 symmetry；历史读取器按两个 section 分开。两者分组语义不同。 |
| 本地生成的 3×3×3 结构化 CGNS | `vtkStructuredGrid`，27 点、8 单元，节点压力范围 100～126 | 当前适配器明确拒绝结构化 Zone。该例只验证基础结构化读取，不代表复杂结构化边界或时序均可用。 |
| 不存在的文件 | 输出错误并返回空多块数据集 | 原型直接检查 `GetErrorCode()` 仍得到 0。接入时必须验证如何捕获 VTK 错误事件、读取结果和空数据，不能仅以该返回值判断成功。 |

M6 流场中，历史读取器的每个边界片区使用整个 Zone 的 107973 个节点和字段数组；例如 `wing_up` 片区有 3079 个面，实际只引用 1625 个节点。直接使用整个节点数组的 `GetRange()` 会得到全体 Zone 压力范围 `540.761～2.3465e+06`，而实际翼面节点范围为 `33283.8～2.3465e+06`。直接按整个点集算边界包围盒也可能包含未被片区单元使用的节点。接入层应针对引用节点进行压缩或准确计算区域范围、包围盒；不能把多块数据直接当作当前精简边界对象使用。

以上数值为输出的打印精度下的比较，尚未逐点比较完整数组、单元连接顺序、方向或渲染结果。

## 后续建议与未验证项

优先将历史读取器作为独立候选后端，保留 `DataAdapter`；为 VTK 多块数据提供通用遍历和边界映射，明确 ZoneBC / element section、节点范围和单位等语义，不按文件名推断。先确定并评审其来源、许可、CGNS API 兼容方案及依赖的封装方式，避免将实验 shim 当成正式实现。ParaView 许可证及各被引入文件和依赖需要在引入时复核。

进一步用合法样例覆盖多 Base/Zone、结构化及非结构化边界、不同字段位置、混合/高阶单元、多解和时间序列、错误与不完整文件；同时完成 Windows VS2017 v141 x64 Debug 的干净编译及真实文件对比，确认 VTK、CGNS、HDF5 编译器和 ABI 一致。对任一格式的支持均以实际样例与自动测试为准，不承诺覆盖所有 CGNS 变体。

## 补充：原配 CGNS 3.3.1 实验

ParaView 5.6.3 随附的 CGNS 头文件声明 `CGNS_VERSION 3310`。另在隔离目录使用 CGNS 官方 v3.3.1（提交 `1a2e5e01284b40841e8b3ede3114f927fa3d769e`）、64 位索引和本机 x86_64 HDF5 1.14.4 编译静态 CGNS 库，并将其与历史读取器及 VTK 8.2.0 链接。该组合不使用前述 CGNS 4.4 的 `cgio_*` 兼容包装。

实验中，M6 Ma5 流场读取的块结构、单元数、字段及打印出来的范围与 4.4 兼容包装实验输出一致；M6 网格和 3×3×3 结构化样例也读取成功。只比较了列出的文本结果，未对数组和拓扑逐项做全量比较，也不能推断 3.3.1 兼容所有后来版本的 CGNS 文件。

编译细节：CGNS 3.3.1 的 CMake 配置显式采用旧策略，本机 CMake 4.5 不接受该策略，改用本机 CMake 3.31.10。其旧 HDF5 查找配置在本机产生空的共享链接库列表，完整共享库构建失败；验证程序使用成功生成的静态 CGNS 库并显式链接 HDF5 动态库，因此尚未完成可复用的正式安装配置。HDF5 1.14.4、VTK 8.2 与该旧库组合只在本机有限样例验证，Windows Debug 和项目其他使用 CGNS 的调用方均未验证。切换正式 CGNS 版本前，必须统一整进程的库版本、头文件、索引位宽和 ABI，并覆盖新版本文件及完整现有读入测试。

## 本地实验后端及公开数据验证（2026-11-15）

本地已将 ParaView 5.6.3 的历史读取器接入为可选后端，CGNS 继续使用 4.4；默认仍是原读取器。启动方式：

```sh
AEROSIM_CGNS_BACKEND=paraview ./scripts/run-postprocess-macos.sh
```

脚本会启用 `AEROSIM_BUILD_PARAVIEW_CGNS_READER`。源码及许可证位于 `modules/visualization/data/third_party/paraview-5.6.3/`。兼容层通过 CGNS 4.4 的类型化 CGIO 接口适配历史 API，尚需正式审查；适配器遍历读取器的多块数据，并按 CGNS 元数据将受支持的非结构 FaceCenter 边界映射到对应单元。未匹配的面保留为 section；并非所有边界编码、多个时间步、所有单元类型都已经支持或验证。为了避免边界使用整片 Zone 的节点影响范围与包围盒，边界仅保留实际引用的节点。矢量字段使用模长着色。

使用 [CGNS 官方样例页](https://cgns.org/current/examples.html)提供的 [tut21.cgns.gz](https://cgns.org/_downloads/002aa6956d52f8fb58bbb290aae3832b/tut21.cgns.gz)，解压后 SHA-256 为 `e9111c20775f249ac96785f7f34e747446284524c5d9424436565a4a30b08cf4`。本机 `cgnscheck -v` 成功，报告了 16 条元数据警告，不能据此宣称样例完全符合所有规范。检查结果为 2106 个点、1584 个体单元；虽样例页描述无边界信息，文件实际包含 PipeWall、PipeInlet、PipeOutlet 三个 FaceCenter 边界，分别为 832、64、64 个面。

通过工作台实际导入后观察到一个体区域与上述三个边界，体区域有 Pressure 等 10 个单元字段，选中 Pressure 后渲染得到着色弯管。自动测试使用 CGNS 4.4 生成含两个边界与已知单元字段值的非结构网格，并覆盖一个基础结构化网格和缺失文件；原读取器现有夹具测试通过，M6 流场分别用新旧后端测试通过。以上仅为本机 macOS 样例验证，Windows VS2017 Debug、复杂结构化边界、时序和更多真实数据仍需验证；不要将实验后端视为覆盖全部 CGNS 变体。

## 大文件本机试验

使用 NASA DPW5 的 L3 多块网格，来源 https://dpw.larc.nasa.gov/DPW5/multiblock_grids.REV01/CGNS/ ，文件 `L3.M.rev01.cgns` 大小约 121 MiB，SHA-256 `a86a86a0604cb563c6405d01658337d671674934a0d77fc5a9bbe8106afe79d3`。其包含 5 个结构化 Zone，不含流场解。通过工作台导入请求与渲染控件运行，选中全部区域，数据显示出 5 个 Zone。外层计算域遮挡了内部几何；这次最初使用旧读取器时，部分 BC_t 类型被跳过；后续结构化 PointRange 通用支持见下文。

在 16 GiB macOS、本机 x86_64 VTK 8.2 / CGNS 4.4 上的一次测试：从发出导入请求到数据显示在树上约 0.71 秒，测试进程从启动到渲染截图及退出约 3.54 秒；`/usr/bin/time -l` 给出的 peak resident set 约 340 MiB，peak memory footprint 约 474 MiB。这些指标属于整个测试进程，不是严格的纯导入增量，不含人工交互帧率测试，不可外推其他数据和 Windows。

实验发现读取器按原顺序选择 Zone、Family 和 Patches 后，部分 Zone 网格被取消选择；改为最后重新选择全部 Base 后，DPW5 文件中的 5 个 Zone 及 CGNS 官方 DLR-F6 文件中的 26 个 Zone 均能列出。进一步检查发现 SIL 的父节点显示为选中时，其子 Grid 仍可能未选中，因此现已逐个选中 Grid；本机生成的双 Zone 结构化夹具已回归通过。历史读取器此前只从每个 Base 的首个 Zone 发现字段名称，现改为扫描所有 Zone；双 Zone 夹具分别写入不同名称及数值的单元字段，读取与数值断言通过。其他 CGNS 变体仍需覆盖，Zone 数量正确不能代替逐元素数据正确性验证。

NASA 121 MiB 文件使用 `cgnscheck` 与文件树检查，5 个 Zone 均有坐标及边界定义，没有 FlowSolution；因此界面没有压力、速度字段符合文件内容。`cgnscheck` 输出 23 条警告。历史读取器此前会跳过部分结构化 BC 类型；后续改动见下文。读取器当前每个 Zone 最多扫描前三个 FlowSolution，其他位置及复杂解仍需验证。

## 飞机用例补充

CGNS 官方 YF-17 HDF5 样例（https://cgns.org/current/examples.html ，解压约 17 MiB，SHA-256 `e1f3ef8f0dd314acb5130dd91d616d405d80af95ee9a0905c2051a614c37665b`）：97,104 点、528,915 体单元，导入显示一个体区域及 14 个边界区域。通过只显示 intake、exhaust、intake ramp、strake、vertical tail、horizontal tail、wing、fuselage 可看到半机外形；这些区域能显示源文件的节点 Pressure。测试按区域独立设置色标范围，颜色不能跨区域直接定量比较。`cgnscheck` 给出 9 条警告。

NASA DPW3 混合单元用例 `dpw_w2_c_at1.ad.2.cgns`（https://dpw.larc.nasa.gov/DPW3/hybrid_DLR/dpw_w2/ ，解压 179,879,936 字节，SHA-256 `0d77bc18955374067a685b405bac10ea395f86abb86551eb7ea58accbee305cd`）：1,928,405 点和 7,141,304 体单元。实验后端导入并渲染了一个体区域和 TRI_3、QUAD_4 两个未分类表面段；树刷新约 3.21 秒，含启动及截图的进程总时长约 5.05 秒，峰值驻留内存 1,514,041,344 字节，峰值 footprint 1,712,018,768 字节。该网格无 FlowSolution。文件包含 3 个 BC，但 `cgnscheck` 对各 BC 的所有 ElementList 项报告单元维度无效，不能可靠映射到飞机表面；该样例尚不能用于验收边界导入正确性。

## 通用边界检查与结构化边界改进

针对历史读取器的限制，实验后端现在支持不依赖 BC 物理类型的结构化 Vertex PointRange 面边界：检查点范围长度、坐标轴及 Zone 范围，必要时规范化逆序范围；其他尚不支持的位置、点集或几何范围作为导入错误返回，不再静默跳过。未结构化边界按文件中的全局单元编号匹配 section；无效、重复、未映射到实际表面输出的引用会返回错误，不尝试猜测编号偏移。对于部分 BC 已映射而剩余面未映射的 section，保留未分类 section。

DPW5 L3 的 5 个体区域与 18 个 PointRange 边界已导入；仅显示五个壁面边界可观察机身与机翼。使用 151.7 MiB 的 DPW5 L2 prism 非结构网格时，体区域及 18 个边界可列出。YF-17 和 M6 流场样例的实验后端导入通过，构造的有效结构化壁面与引用体单元的无效边界测试通过。DPW3 的旧文件三个 BC 引用的整数与全局表面单元编号不匹配，现明确返回错误；源文件未修改，没有猜测或应用偏移。元数据缺失类 `cgnscheck` 警告仍属于源文件信息，并不通过自行补造单位或量纲来消除。
