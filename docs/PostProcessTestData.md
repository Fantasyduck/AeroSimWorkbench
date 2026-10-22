# 后处理解析测试夹具

目录：`tests/fixtures/postprocess/`。运行 `python3 tests/fixtures/postprocess/generate.py` 生成数据；`python3 tests/fixtures/postprocess/generate.py --check` 比对全部七个已提交文件并检查解析值。生成器仅用 Python 标准库，兼容 Windows Python 3.6；CMake 找到 Python3 时会注册 `postprocess_fixture_check` CTest。项目不用 Python 作为生产数据解析依赖。

## 内容、语义与期望

| 文件 | 格式与内容 | 可检查的结果 |
| --- | --- | --- |
| `mesh.json` | 自描述**夹具专用** JSON：27 个节点，x 最快、y 次之、z 最慢；8 个有序八节点六面体；六个 boundaryId=1…6，每组四个四边形面；坐标单位 m。 | 立方体范围 [-1,1]³，单元数 8，x=0 有 9 个网格节点。JSON 只是核对拓扑/边界的独立描述，不宣称是已定生产网格 schema。 |
| `cube_t00.vtk`、`cube_t01.vtk`、`cube_t02.vtk` | 真正的 VTK legacy ASCII `STRUCTURED_POINTS`，3×3×3，原点 (-1,-1,-1)，间距 1；27 个 point values、8 个隐式体单元。 | t=0/1/2 秒，四个点场：Pressure=100000+1000x+100t Pa；Temperature=300+10y K；Phi=x m；Velocity=(1,0,0) m/s。中心点 t=2 为 (100200 Pa, 300 K, 0 m, 1/0/0 m/s)。 |
| `index.json` | 夹具专用文件/帧索引、字段元数据（单位、点关联）、datasetId 和 fileId；三个有序有效帧。 | 同一数据集 `cube-run-1`，fileId 各异，物理时间 0/1/2 s；用于讨论正式协议，不是已有生产数据目录。 |
| `cases.csv` | 四组 caseId、alpha/beta/delta、动压 1000 Pa、面积 2 m²、参考长度 0.5 m，body-xyz 下 Fx/Fy/Fz (N)、Mx/My/Mz (N·m)。 | case-02: α=5°, δ=10°, Fx=170, Fy=0, Fz=130, Mx=0, My=20, Mz=0；case-01 同 α/δ=0，用来讨论舵偏差。 |
| `curves.csv` | 四个点组成一个示例 series，x=alpha (deg)、y=Cz 无量纲，保留 caseId。 | 定义仅为 `Cz=Fz/(q S)`，case-02=0.065；**不是**已定气动力符号规范。相同 α 的不同舵偏 case 不能无声覆盖。 |

等值测试建议：`Phi=0` 的几何是 x=0 平面；在 z=0 物面/切面上 `Pressure=100000 Pa` 是 x=0 线。切面测试 x=0，点网格交线覆盖 y/z=-1..1；VTK filter 三角化/去重可能改变点/单元计数，因此验收应检查几何范围和数值，不把 filter 输出单元数强定为接口。速度积分 `(1,0,0)` 应沿 +x，物面约束和积分步长由专项 D 说明。三帧压力中心依次 100000/100100/100200 Pa。

注意：VTK 的 legacy reader 如 `vtkStructuredPointsReader` 默认可能只载入一个标量；测试需启用 `ReadAllScalarsOn()`（并按需 `ReadAllVectorsOn()`）才能取得 Temperature、Phi 和 Velocity。`mesh.json` 显式边界元信息与 `.vtk` 隐式结构化网格配套，`.vtk` 本身不编码六个边界 ID；专项 A 必须解释读入生产格式时的边界保留方式。

## 可复现检查

```bash
python3 tests/fixtures/postprocess/generate.py --check
# 有 Python vtk 时还可运行 VTK 文件读取/解析场检查：
python3 tests/fixtures/postprocess/verify_vtk.py
# 在 Qt 5.12.12 已安装的机器配置主工程后：
ctest --test-dir build-windows -C Debug --output-on-failure
```

生成脚本不会产出 MSH/CAS/CGNS、Tecplot/Plot3D 或 Excel 文件。这些格式需要成员提供确切版本、合法生成/读取库和往返测试。大型真实仿真数据仅记录“求解器及版本、文件格式版本、Zone/边界/单位、许可/脱敏、获取位置、期望结果”，保存在受控位置；不要提交到 Git，也不要给普通文本改一个专业后缀。AI 仅用四组数据验证列映射和错误路径，不声称训练/泛化达标。

## 本次实测（2026-10-22）

- 目标 Windows 10 / VS2017 v141 x64 / Qt 5.12.12：`scripts\build-windows.cmd all Debug` 配置与编译通过；CTest **3/3**，其中 `postprocess_fixture_check` 通过。Demo 虚拟环境 Python VTK **9.3.0** 运行 `verify_vtk.py`，三帧、四字段、切面/等值面/流线数值检查通过。
- 本地 macOS：生成器 `--check` 通过；Python VTK **9.7.1** 的同一读取/几何检查通过。本地 CMake 配置因仅安装 Qt **5.15.19**，不符合工程的 `Qt 5.12.12 EXACT` 而失败；没有宣称本机 C++ 构建成功。
- **尚未验证**：正式 C++ VTK 8.2 读取这些文件、真实 CGNS/MSH/CAS、Windows Release、成员的任何算法实现或界面集成。上述 Python VTK 检查只证明夹具是可读取的解析数据，并能触发对应的 VTK 功能。
