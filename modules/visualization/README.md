# 可视化与网格/结果

本模块包含正式 C++/Qt 5.12.12/VTK 8.2 视图、网格/结果和空间后处理的接入位置。当前分支新增 `aerosim_spatial_features` 算法库及 `aerosim_spatial_features_test` 命令行测试，覆盖任务 ④⑤⑥⑭ 的几何处理和 VTP 导出。**Viewer、对象显隐/删除、工程数据访问及完整界面尚未接入。**

## 交付接口

公开头文件位于 `include/aerosim/visualization/features/`，实现位于 `src/features/`，命名空间为 `aerosim::visualization`。

| 接口 | 输入与输出 |
|---|---|
| `FeatureProcessor::slice` / `slices` | 输入体网格及一个/多个位置、法向，返回相互独立的切面结果；输入不被修改。批量返回与参数顺序一一对应，每项独立报告错误。 |
| `FeatureProcessor::contourLines` | 输入调用方明确选中的二维物面或切片、标量选择和等值，输出线几何。拒绝直接把整个体网格当成物面。 |
| `FeatureProcessor::isosurface` | 输入体网格、标量选择和等值，输出面几何。 |
| `FeatureProcessor::snapshot` | 为调用方提供的 `vtkPolyData` 建立独立快照，也可用于接收其他模块生成的流线等特征。 |
| `FeatureGeometry` | 私有拥有几何副本，通过 `shared_ptr<const FeatureGeometry>` 交付；`copyData()` 返回可独立使用的深拷贝，不暴露原始可写指针。 |
| `FeatureExporter::writeVtp` | 输出真实 `.vtp`，可选择点/单元数组，默认保留全部数组；通过 `QSaveFile` 支持中文路径与原子提交。默认拒绝覆盖，需显式指定 `overwrite=true`。 |
| `FeatureJob` | 可选后台适配：提交时复制输入，工作线程调用算法；`ready()` 查询，`take()` 取一次结果，`cancel()` 丢弃尚未接收的结果。 |

以上为模块内调用 API，不是新增的跨模块公共契约。没有修改 `contracts/`，没有自建工程/数据集 ID、Renderer 或场景注册表。正式接入时由适配层把已有 `DatasetRef`/`MeshRef` 与输入快照关联，并将结果交给经评审的统一场景入口。

## 最小调用

```cpp
using namespace aerosim::visualization;
SliceParameters plane;
plane.origin = {{0, 0, 0}};
plane.normal = {{0, 0, 1}};
auto slice = FeatureProcessor::slice(volume, plane);
if (!slice.ok()) {
    // 将 slice.status.code/message 交给日志或 UI，不在算法中弹窗。
    return;
}
ScalarSelection field;
field.name = "Pressure";
auto line = FeatureProcessor::contourLines(slice.geometry->copyData(), field, 100000);
if (line.ok()) {
    auto status = FeatureExporter::writeVtp(*line.geometry, QStringLiteral("line.vtp"));
    // 检查 status.ok()；失败不会截断已有文件。
}
```

每次调用无全局状态。多截面的显示、重算替换和删除由未来场景层持有多个结果实现，本库不处理相机、Actor、颜色或显示列表。上例默认点标量；数值使用输入坐标和字段单位，不做单位转换。

## 数据、错误和线程规则

- 当前接受单个 VTK 数据集。切片/等值面要求全部单元为三维；等值线要求全部单元为二维。混合维度、多 Zone、边界分类需由上游选择/规范化，不能将数据集整个外边界默认为飞行器壁面。
- 等值提取仅支持数值型单分量标量，校验元组数量、有限值和字段存在性。`ScalarSelection` 明确点/单元关联；单元值默认拒绝插值，显式选择 `AverageToPoints` 后才使用 VTK 邻接平均。同名点/单元字段发生冲突时返回错误，要求上游消歧。
- 常量标量返回 `DegenerateField`；越界等值或无交集返回 `EmptyResult`，不会构造可用的空句柄。切面法向支持非单位长度，但不能为零或 NaN/Inf。接收的数据集必须由上游保证连接关系合法。
- 切片保留 VTK 能传递的字段；等值线/面还可能附带 VTK 计算的数组。导出保留或筛选已有数组，不重新计算数值，不隐式变更单位。
- 同步算法可直接在无界面测试中运行；大数据应在工作线程调用。`FeatureJob` 不发送 Qt 信号、不操作渲染对象；未来通过 Qt 跨线程接入时遵守 `Qt::QueuedConnection` 及元类型注册规范。
- 输入在同步调用或 `submit()` 复制期间不得被其他线程修改。提交后外部输入可释放/修改；工作端只使用私有快照。结果只读，消费端拿到副本后独占修改。
- 取消是**结果丢弃**，不是中途终止 VTK。来源卸载/参数变更时调用 `cancel()`；完成后再 `take()`。销毁未完成任务会等待，GUI 退出应先取消并异步等候完成，不能在 GUI 线程直接销毁长任务。无界面调用可直接等待。
- 当前不提供统一线程池或项目版本系统；并发数量由调用方限制。快照复制和 ASCII VTP 序列化需要额外内存，不宣称已优化超大网格。
- 文件写出是同步接口，可由调用方工作线程调用。暂不支持 Tecplot/CGNS/Plot3D；不允许仅更换后缀伪装格式。同一目标路径的并发写入需由调用方串行化。

## 独立构建与测试

Windows 最简步骤见 [tests/README.md](tests/README.md)，入口为仓库根目录的 `scripts/test-spatial-features.cmd`。测试不需要界面和求解器。

也可以自行配置：

```bat
cmake -S modules/visualization -B build-spatial-features -G "Visual Studio 15 2017 Win64" -DCMAKE_PREFIX_PATH="%AEROSIM_QT_ROOT%" -DVTK_DIR="%AEROSIM_VTK_DIR%"
cmake --build build-spatial-features --config Debug --target aerosim_spatial_features_test
```

根工程可选 `-DAEROSIM_BUILD_SPATIAL_FEATURES=ON` 接入同一目标和测试，默认 OFF，保证不安装 VTK 的骨架和 CLI 不受影响。通过统一脚本构建时，先在该构建目录配置此选项及 `VTK_DIR`，脚本后续沿用 CMake 缓存；或直接使用独立测试脚本。

依赖：C++14、Qt Core 5.12.12（项目既有依赖，按原 Qt 授权）、VTK 8.2.0（BSD-3-Clause，Kitware 官方发行及项目既有安装）、平台线程库。没有新增第三方库。只有使用该模块时查找 VTK，不链接 Widgets/OpenGL/SARibbon。MSVC 仅允许 Debug，对应目标机已有库。

## 验证状态

本地辅助验证：macOS arm64、AppleClang 17、C++14、Qt Core 5.15.15、VTK 8.2.0 Debug，直接编译本仓库算法与测试源码，9 组测试全部通过。VTK 使用官方 8.2.0 源码构建的计算模块；本地独立验证配置放在仓库外，未放宽正式 CMake 的 Qt 5.12.12 / VTK 8.2.0 精确版本要求。

Windows VS2017 / Qt 5.12.12 / VTK 8.2.0 Debug 由使用者手动运行上述脚本验证；本次未远程执行 Windows 测试。本地辅助验证不替代目标 Windows 验收。
