# CGNS 数据读取（开发中）

本目录提供仅供可视化模块使用的格式适配器接口和 CGNS 实验实现，不修改公共契约；数据中持有 VTK 数据对象，不依赖 QWidget。当前实现仅支持不混合的基本线性非结构单元、单个解的节点/单元字段；不支持结构化区域、MIXED/NGON、高阶单元、多解或时间序列，遇到不支持的内容返回错误，不应声称通用 CGNS 支持。面中心 ZoneBC 的 PointRange/PointList 按单元编号映射成边界区域；不支持的边界位置以及未覆盖的低维单元仍以单元段显示，不推断物理边界；单元字段仅用于体区域，不伪造边界数据。单位和时间不推断。

独立构建需要 Qt 5.12.12、VTK 8.2.0 Debug 和与同一工具链匹配的 CGNS/HDF5 库，通过 `AEROSIM_CGNS_ROOT`、`VTK_DIR`、`CMAKE_PREFIX_PATH` 指定，不把本机路径写入工程。目标机探索性构建使用 CGNS 4.4.0（zlib 许可，源提交 `394de1cb2dee9cc44f7bc3a761c70733ac15bcd1`），MSVC v141 x64 Debug，链接 VTK 8.2 自带的 HDF5 1.10.1；其作为项目依赖的版本、许可及交付方式还需确认。运行时需要 CGNS DLL、匹配 VTK 和 Qt DLL。

`aerosim_cgns_adapter_test <M6 flowfield.cgns>` 使用真实样例核对网格、边界、字段及错误路径，不把它当作全部格式验收。已经接入后处理数据树和 VTK 视口；另有自动生成小型 CGNS 夹具的 `cgns_fixture_import` 测试。派生对象计算仍未接入。

Windows 后处理运行和构建使用现有的 `scripts/run-postprocess-windows.cmd` 和 `scripts/build-windows.cmd aerosim_postprocess Debug`。依赖路径可放在不提交的 `aerosim-windows.local.cmd` 中，或通过环境变量提供：`AEROSIM_QT_ROOT`、`AEROSIM_SARIBBON_SOURCE_DIR`、`AEROSIM_VTK_DIR`、`AEROSIM_VTK_BIN` 和 `AEROSIM_CGNS_ROOT`。运行脚本会启用可视化；其他目标未显式指定该选项时保留 CMake 缓存。构建与运行都使用匹配的 Debug 依赖，CGNS 的 HDF5 必须与 VTK 所用版本及构建方式兼容。

## VTK 与 Tecplot 导入

工作台的导入路径还使用 VTK 自带的 `vtkGenericDataObjectReader`、`vtkXMLGenericDataObjectReader`、`vtkXMLMultiBlockDataReader` 和 `vtkTecplotReader`。Legacy `.vtk`、XML `.vti/.vtu/.vts/.vtr/.vtp/.vtm` 及 Tecplot ASCII `.tec/.dat` 按区域保存为 `vtkDataSet`，保留点和单元数组；多块文件保留独立区域与名称，不推断物理边界。Legacy reader 启用全部标量、矢量、张量和字段数组。显示层仍使用现有异步读取与追加/替换流程。文件缺失、空数据、reader 报错或不支持的块类型返回错误。

VTK 8.2 的 Tecplot reader 是简化 ASCII 实现，支持范围取决于 VTK reader；不支持 Tecplot 二进制 `.plt/.szplt`，也不承诺全部 Tecplot 特性或时间序列。Tecplot 的变量不会根据名称猜测合成为速度矢量。VTK 表格、图、并行 XML、时序及非 `vtkDataSet` 块不在本次范围内。多 Zone 不合并以免丢失区域或字段关联。

公开测试数据来自 VTK 8.2 测试数据对象库，下载到本地忽略目录 `build-public-reader-data/`，不提交大数据；对应索引可见 [VTK 8.2 测试数据](https://github.com/Kitware/VTK/tree/v8.2.0/Testing/Data)：

| 文件 | SHA-512 | 用途 |
| --- | --- | --- |
| `ironProt.vtk` | `2fb043a2e0007d35d33bc4840b017b7b012dfacf62ca08ae03d765f3d9f3dad1e3ff24eca884db9ab9eb5d1c1fb1021b7b4256aa7973252819d19dacee58c366` | Legacy VTK |
| `flow.tec` | `5fac4720c813574e4ac90b3f9b9eab3cd47c375a221a67996190ec59a858dc220c569ee701d23dff8232c0cb13e8158e247f1de4a71b76ddb2c44bdbd454f788` | 非结构网格与节点字段 |
| `cellcentered.tec` | `77e248f3e057e3210d9c235215b8cb57528d830336f96a66088c8c3481ac4c486b4c1e9b509fd0df9053df3666ff5c14d7dd21e11b18eacee09747087d94fd62` | 多 Zone、节点及单元字段 |

可通过 `https://data.kitware.com/api/v1/file/hashsum/SHA512/<上述摘要>/download` 下载各文件。配置 `AEROSIM_READER_TEST_DATA_DIR` 指向样例目录后，CTest 注册 `vtk_tecplot_adapter_import`；测试还用仓库的 cube 夹具检查多数组，并通过写出后读回检查 XML 和多块数据。启用可视化后还注册 `vtk_tecplot_workbench_import`，通过工作台导入对话框检查数据树、字段和渲染对象。

### 真实数据补充验证（macOS，VTK 8.2）

- [Zenodo spinSdvection](https://zenodo.org/records/16995252) 的 `aorta_0.vtu`（118.2 MB，MD5 `72d4e1f0f340b82cf3c6cbe2eabc00c1`）：1,257,200 点、1,193,964 单元，点和单元各有三分量 `U`；工作台导入和 actor 创建通过。
- [Zenodo 河道水动力模型](https://zenodo.org/records/22096964) 的 `Case1.dat`：原文件变量名未加引号且首行写为 `Titl=`。VTK 8.2 会在未加引号变量列表上循环，当前适配器提前报错；将副本首行规范为 `TITLE="Calculation results"`、变量名加引号后，13,544 点、26,450 个单元及 9 个节点字段通过工作台导入。数值、拓扑未改；`SOLUTIONTIME` 不受 VTK 8.2 reader 支持，时间未导入。该副本不是原始文件兼容性通过的证明。
