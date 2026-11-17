# CGNS 数据读取（开发中）

本目录提供仅供可视化模块使用的格式适配器接口和 CGNS 实验实现，不修改公共契约；数据中持有 VTK 数据对象，不依赖 QWidget。当前实现仅支持不混合的基本线性非结构单元、单个解的节点/单元字段；不支持结构化区域、MIXED/NGON、高阶单元、多解或时间序列，遇到不支持的内容返回错误，不应声称通用 CGNS 支持。面中心 ZoneBC 的 PointRange/PointList 按单元编号映射成边界区域；不支持的边界位置以及未覆盖的低维单元仍以单元段显示，不推断物理边界；单元字段仅用于体区域，不伪造边界数据。单位和时间不推断。

独立构建需要 Qt 5.12.12、VTK 8.2.0 Debug 和与同一工具链匹配的 CGNS/HDF5 库，通过 `AEROSIM_CGNS_ROOT`、`VTK_DIR`、`CMAKE_PREFIX_PATH` 指定，不把本机路径写入工程。目标机探索性构建使用 CGNS 4.4.0（zlib 许可，源提交 `394de1cb2dee9cc44f7bc3a761c70733ac15bcd1`），MSVC v141 x64 Debug，链接 VTK 8.2 自带的 HDF5 1.10.1；其作为项目依赖的版本、许可及交付方式还需确认。运行时需要 CGNS DLL、匹配 VTK 和 Qt DLL。

`aerosim_cgns_adapter_test <M6 flowfield.cgns>` 使用真实样例核对网格、边界、字段及错误路径，不把它当作全部格式验收。已经接入后处理数据树和 VTK 视口；另有自动生成小型 CGNS 夹具的 `cgns_fixture_import` 测试。派生对象计算仍未接入。

Windows 后处理运行和构建使用现有的 `scripts/run-postprocess-windows.cmd` 和 `scripts/build-windows.cmd aerosim_postprocess Debug`。依赖路径可放在不提交的 `aerosim-windows.local.cmd` 中，或通过环境变量提供：`AEROSIM_QT_ROOT`、`AEROSIM_SARIBBON_SOURCE_DIR`、`AEROSIM_VTK_DIR`、`AEROSIM_VTK_BIN` 和 `AEROSIM_CGNS_ROOT`。运行脚本会启用可视化；其他目标未显式指定该选项时保留 CMake 缓存。构建与运行都使用匹配的 Debug 依赖，CGNS 的 HDF5 必须与 VTK 所用版本及构建方式兼容。
