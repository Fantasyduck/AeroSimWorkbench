# 控制方程参数映射表

XCF 路径相对于根节点 `DATA_ROOT`；样例值取自 `M6.xcf`。

| 界面参数／选项 | 工作台内部值 | XCF 路径 | 映射编码 |
| --- | --- | --- | --- |
| 定常计算 | `calculationType=steady` | `EquationSettingData/ISSTEADY` | 0 |
| 非定常计算 | `calculationType=unsteady` | `EquationSettingData/ISSTEADY` | 1 |
| 欧拉方程／无粘流 | `equation=euler`、`flowRegime=inviscid` | `EquationSettingData/EQUATION_TYPE` | 1 |
| N-S 方程／层流 | `equation=navier-stokes`、`flowRegime=laminar` | `EquationSettingData/EQUATION_TYPE` | 0 |
| N-S 方程／湍流 | `equation=navier-stokes`、`flowRegime=turbulent` | `EquationSettingData/EQUATION_TYPE` | 0 |
| 流态：层流 | `flowRegime=laminar` | `EquationSettingData/TURB_MODEL` | 0 |
| 流态：湍流 | `flowRegime=turbulent` | `EquationSettingData/TURB_MODEL` | 1（所有湍流模型） |
| 流态：欧拉／无粘流 | `flowRegime=inviscid` | `EquationSettingData/TURB_MODEL` | 2 |
| 湍流模型：S-A | `turbulenceModel=sa` | `TurbModelData/TURB_MODEL` | 0 |
| 湍流模型：k_w Menter | `turbulenceModel=kw-menter` | TurbModelData/TURB_MODEL | 1 |
| 湍流模型：des | `turbulenceModel=des` | TurbModelData/TURB_MODEL | 2 |
| 湍流模型：ddes | `turbulenceModel=ddes` | TurbModelData/TURB_MODEL | 3 |
| 湍流模型：wilcox k_w1998 | `turbulenceModel=wilcox-kw1998` | TurbModelData/TURB_MODEL | 4 |
| 湍流模型：wilcox k_w2006 | `turbulenceModel=wilcox-kw2006` | TurbModelData/TURB_MODEL | 5 |
| 不考虑稀薄气体效应 | `rarefiedGas=false` | `EquationSettingData/Considering_rarefied_gas` | 0 |
| 考虑稀薄气体效应 | `rarefiedGas=true` | `EquationSettingData/Considering_rarefied_gas` | 1 |
| 碰撞方程：BGK | `collisionEquation=bgk` | 无独立字段 | 仅一种，默认选中 |
| 子模型：标准 BGK | `collisionSubmodel=standard-bgk` | `EquationSettingData/ZimoxingTYPE` | 0 |
| 子模型：ES-BGK | `collisionSubmodel=es-bgk` | `EquationSettingData/ZimoxingTYPE` | 1 |
| 克努森数 Kn | `knudsenNumber` | `EquationSettingData/NusenShu` | 直接数值对应 |
| 来流粒子参考数 | `freestreamParticleCount` | `EquationSettingData/LailiuLizi` | 直接数值对应 |
| 网格粒子参考数 | `gridParticleCount` | `EquationSettingData/WanggeLizi` | 直接数值对应 |
| 碰撞模型：VSS | `collisionModel=vss` | `EquationSettingData/PengZhuang` | 0 |
| 碰撞模型：HS | `collisionModel=hs` | `EquationSettingData/PengZhuang` | 1 |
| 碰撞模型：VHS | `collisionModel=vhs` | `EquationSettingData/PengZhuang` | 2 |
| 当前界面无对应控件 | — | `EquationSettingData/PERIODIC_BODY_FORCE` | 含义待确认；保存时保留 |

## 计算控制参数映射表（明确项已接入读写）

表中 `R` 表示 `/DATA_ROOT/RegionBlkMG/BasicRegion/_reg3`；其他路径相对于 `/DATA_ROOT`。

| 界面参数／选项 | 工作台内部值 | XCF 候选路径 | 当前样例值 | 映射关系（请确认／补充） |
| --- | --- | --- | --- | --- |
| 一阶迭代步数 | `firstOrderIterations` | `R/FOIS` | `0` | 直接整数对应 |
| 最大迭代步数 | `maximumIterations` | `R/NITSCG` | `882`（用户示例） | 直接整数对应；`NITSFG` 保留原值 |
| 收敛残差 | `convergenceResidual` | `R/dABS_RES` | `-10` | 直接实数对应；`dREL_RES`、`ISABS_RES` 保留原值 |
| 残差监控阈值 | `residualMonitorThreshold` | `R/rms_limit_upper` | `100` | 直接实数对应 |
| 不使用 GPU 计算 | `useGpu=false` | `R/ISGPU` | `0` | 0 |
| 使用 GPU 计算 | `useGpu=true` | `R/ISGPU` | `0` | 1 |
| 计算进程数 | `processCount` | `Geom_Type_Dat/_nProcess` | `4` | 进程数写 `_nProcess`；`_nMPI` 保留原值 |
| GPU 卡数（启用 GPU 时显示） | `gpuCount`，整数，默认 `1` | `R/GPUNUM` | `1` | 直接整数对应 |
| 指定计算 GPU（启用 GPU 时显示） | `gpuIndex`，文本，默认空 | `R/GPUIndex` | `4294967295` | 尚未接入读写，保留原 XML 值；编号起点、多卡编码及空选项编码待确认 |
| 当前界面无对应控件 | — | `R/STARTNUM` | 空 | 暂保留；与 GPU 选择是否有关待确认 |

## 边界条件参数映射表（待确认，尚未接入读写）

| 路径缩写 | XCF 路径（相对于 `/DATA_ROOT`） |
| --- | --- |
| `B` | `BCDataManager/BC_DATA`，每个边界一项 |
| `F` | `B/_farfieldParas` |
| `I` | `BCInfoManager/BCINFOS/BC_INFO` |
| `G` | `BCInfoManager/BCGROUPS/GROUP` |
| 内部值前缀 | 下表 `name/type/farfield/typeParameters` 均属于 `boundaries[]` 中的当前边界 |

### 边界列表及关联

| 界面参数／关联 | 工作台内部值 | XCF 字段 | 当前样例值 | 映射关系／待确认 |
| --- | --- | --- | --- | --- |
| 边界列表 | `boundaries[]` | `B` | 5 项 | 从文件建立列表；当前 UI 使用固定 M6 示例 |
| 边界标识 | `id`（当前示例为名称字符串） | `B/_nBCUnit` | `1～5` | 持久标识；与 `G/id`、`I/groupid` 对应，是否跨网格唯一待确认 |
| 边界名称 | `name` | `B/_ssName` | `farfield`、`symmetry`、`wing_down` 等 | 直接文本对应；重命名时修改所有名称 |
| 分组名称 | 无独立控件 | `G/name` | 同边界名称 | 按 `G/id = B/_nBCUnit` 关联，重命名同步规则待确认 |
| 网格边界名称 | 无独立控件 | `I/name` | 同边界名称 | 按 `I/groupid = B/_nBCUnit` 关联；是否允许随配置名修改待确认 |
| 网格边界索引 | 无独立控件 | `I/index` | `1～5`；另有 `interface=0` | 不应直接当作列表行号；与网格原始边界关联规则待确认 |
| 所属网格 | `mesh` | 候选 `GridFormatDat/grid_info/grid_name`；或 `FILE_ROOT/_strName` | 均为 `M6` | `B` 内没有网格名；多网格关联方式待确认 |
| 分组类型 | 无独立控件 | `G/bctype` | 所有物理边界均为 `0`，`interface` 为 `-1` | 不能据此区分远场／壁面／对称面，保留语义待确认 |
| 接口边界 | 当前无对应项 | `I`、`G` 中 `interface` | `groupid/id=-1`；`B` 中没有对应项 | 是否排除在可编辑列表之外待确认 |

### 边界类型

| 界面选项 | 内部 `type` | `B/_ssBCType` 字符串 | 其他编码／待确认 |
| --- | --- | --- | --- |
| 远场 | `farfield` | `farfield`（样例存在） | — |
| 自由来流 | `freestream` | 待确认 | — |
| 对称面 | `symmetry` | `symmetry`（样例存在） | — |
| 重叠网格 | `overset` | 待确认 | — |
| 超声速出口（外推边界） | `supersonic-outlet` | 待确认 | — |
| 无粘壁面 | `inviscid-wall` | `inviscidwall`（样例存在） | 样例 `_ssOption=PHYSICAL`，含义待确认 |
| 无粘壁面（壁面函数） | `inviscid-wall-wall-function` | 待确认 | 是否共用 `inviscidwall` 并另设壁面函数标记待确认 |
| 无滑移绝热壁面 | `no-slip-adiabatic-wall` | 待确认 | XDBA 样例为 `adiabaticwall`；不能直接作为 XCF 确认值 |
| 无滑移绝热壁面（壁面函数） | `no-slip-adiabatic-wall-wall-function` | 待确认 | 壁面函数字段及编码待确认 |
| 无滑移等温壁面 | `no-slip-isothermal-wall` | 待确认 | — |
| 无滑移等温壁面（壁面函数） | `no-slip-isothermal-wall-wall-function` | 待确认 | 壁面函数字段及编码待确认 |
| 质量流量亚音速出口 | `mass-flow-subsonic-outlet` | 待确认 | — |
| 质量流量亚音速入口 | `mass-flow-subsonic-inlet` | 待确认 | — |
| 压力亚音速出口 | `pressure-subsonic-outlet` | 待确认 | — |
| 压力亚音速入口 | `pressure-subsonic-inlet` | 待确认 | — |
| 超音速和亚声速混合压力出口 | `mixed-pressure-outlet` | 待确认 | — |

### 各类型附加参数

| 界面参数 | 内部 `typeParameters/<type>/` 下的键 | 适用类型 | XCF 字段／位置 | 待确认 |
| --- | --- | --- | --- | --- |
| 壁面温度 (K) | `wallTemperature` | 两种等温壁面 | 待确认 | 是否存入 `B/_lstValues`，以及位置、分隔符 |
| 质量流量 | `massFlowRate` | 质量流量亚音速入口／出口 | 待确认 | 字段／列表位置、单位、正负号约定 |
| 总温 (K) | `totalTemperature` | 质量流量亚音速入口 | 待确认 | 字段／列表位置 |
| 压力松弛因子 | `pressureRelaxationFactor` | 压力亚音速入口、混合压力出口 | 待确认 | 字段／列表位置 |
| 出口静压 (Pa) | `outletStaticPressure` | 压力亚音速出口、混合压力出口 | 待确认 | 字段／列表位置 |
| 进口温度 (K) | `inletTemperature` | 压力亚音速入口 | 待确认 | 字段／列表位置；静温还是总温 |
| 进口总压 (Pa) | `inletTotalPressure` | 压力亚音速入口 | 待确认 | 字段／列表位置 |
| 当前无独立控件 | — | 各边界 | `B/_ssOption` | 壁面样例为 `PHYSICAL`，远场／对称面为空；是否包含壁面函数设置 |
| 当前无独立控件 | — | 各边界 | `B/_lstValues`、`B/_lstMoles`、`B/_lstTurs` | 样例均为空；列表用途、位置与编码待确认，接入前保留 |

### 远场来流参数

| 界面参数／选项 | 内部 `farfield/` 下的键 | XCF 字段 | 当前样例值 | 映射关系／待确认 |
| --- | --- | --- | --- | --- |
| 单位雷诺数和静温 | `parameterType=reynolds-temperature` | `F/INPUT_METHOD` | — | 整数编码待确认 |
| 静压和静温 | `parameterType=static-temperature-pressure` | `F/INPUT_METHOD` | `2` | 样例为 2，待确认 |
| 总压和总温 | `parameterType=total-temperature-pressure` | `F/INPUT_METHOD` | — | 整数编码待确认 |
| 高度 | `parameterType=altitude` | `F/INPUT_METHOD` | — | 整数编码待确认 |
| 攻角 (°) | `angleOfAttack` | `F/ALPHA` | `0` | 候选直接数值对应，角度单位待确认 |
| 侧滑角 (°) | `sideslipAngle` | `F/BETA` | `0` | 候选直接数值对应，角度单位待确认 |
| 马赫数 | `machNumber` | `F/MACH_NO` | `6` | 候选直接数值对应 |
| 单位雷诺数 (1/m) | `unitReynoldsNumber` | `F/UNIT_REYNOLDS_NO` | `-1` | 单位、未启用时 `-1` 的含义待确认；当前 UI 最小值为 `0.000001`，后续需保留隐藏原值 |
| 静温 (K) | `staticTemperature` | `F/STATIC_TEMP_VALUE` | `60` | 候选直接数值对应 |
| 静压 (Pa) | `staticPressure` | `F/STATIC_PRESS_VALUE` | `955` | 候选直接数值对应 |
| 总温 (K) | `totalTemperature` | `F/TOTAL_TEMP_VALUE` | `1e3` | 候选直接数值对应；未启用值保留规则待确认 |
| 总压 (Pa) | `totalPressure` | `F/TOTAL_PRESS_VALUE` | `1e4` | 候选直接数值对应；未启用值保留规则待确认 |
| 高度 (m) | `altitude` | `F/HEIGHT_VALUE` | `0` | 候选直接数值对应 |
| 自由来流的参数来源 | 当前 `freestream` 无附加控件 | 待确认 | — | 是否复用参考量，而非 `_farfieldParas` |

## 求解方法参数映射表（确认项已接入读写）

路径相对于 `/DATA_ROOT/SolutionMethodData`；样例值取自当前 `M6.xcf`。

| 界面参数／选项 | 工作台内部值 | XCF 字段 | 当前样例值 | 映射关系 |
| --- | --- | --- | --- | --- |
| 空间离散格式：Roe／Vanleer／AUSM／AUSMPW+／HLLC | `spatialScheme=roe/vanleer/ausm/ausmpw-plus/hllc` | `SPADIS` | `3` | 按顺序为0-4 |
| 考虑熵修正（仅 Roe 显示） | `entropyFix`，默认 `false` | `adptv_entropy_fix` | `1` | 选中1，未选中0；切换格式保留勾选值 |
| 流场限制器：无／vanleer／vanalbada／hvanleer／hvanalbada | `limiter=none/vanleer/vanalbada/hvanleer/hvanalbada` | `FLOWLIM` | `2` | 按顺序为0-4 |
| CFL 渐变点数 | `cflPointCount` | `CFL_VAL` | `2` | 直接对应 |
| CFL 表格：迭代步数 | `cflPoints[].iteration` | `lstLn1` | `1,2000` | 按行以逗号分隔 |
| CFL 表格：CFL 数 | `cflPoints[].cfl` | `lstLn2` | `0.1,5` | 按行以逗号分隔 |
| 开启强保稳 | `strongStability` | `linear_projection` | `0` | 开启1，关闭0 |
| 时间离散格式（非定常） | `timeScheme=bdf1/bdf2/optimized-bdf2/bdf3/modified-bdf4/ssprk3` | `TIMEDIS` | `0` | 按顺序依次0-5 |
| 时间步长（非定常） | `timeStep` | `time_step` | `0.01` | 直接数值对应、单位为秒 |
| 子迭代步数（非定常） | `subIterations` | `subiterations` | `10` | 直接整数对应 |
| 当前界面无对应控件 | — | `TURBCFL_VAL`、`TurblstLn2` | `2`、`0.01,1` | 暂保留 |

## 输出控制参数映射表（确认项已接入读写）

路径相对于 `/DATA_ROOT`。当前 UI 的壁面／流场输出间隔支持 `-1000000000～1000000000`，默认仍为 `-1`；这只是控件范围，不代表已确认文件语义。

| 界面参数 | 工作台内部值 | XCF 候选路径 | 当前样例值 | 映射关系（请确认／补充） |
| --- | --- | --- | --- | --- |
| 续算文件输出间隔 | `restartInterval` | `RefenceParametersData/CONTINUE_INTERVAL` | `111` | 使用continue |
| 壁面结果输出间隔 | `wallInterval` | `RefenceParametersData/WALL_INTERVAL` | `-11` | 整数直接对应 |
| 流场结果输出间隔 | `flowInterval` | `RefenceParametersData/FLOW_INTERVAL` | `-12` | 整数直接对应 |
| 流场输出变量 | `flowVariables`（字符串列表） | `OutputSettingData/VOLUME` | 17 个 `1`，分号分隔 | 已接入：按下表顺序对应 17 个标记；选中1，未选中0 |
| 壁面输出变量 | `wallVariables`（字符串列表） | `OutputSettingData/BOUNDARY` | 17 个 `1`，分号分隔 | 按下表顺序对应 17 个标记；选中1，未选中0 |
| 边界条件收敛历史 | `boundaryHistory`（名称列表） | `OutputSettingData/OUTPUTBC` | `farfield;symmetry;wing_down;wing_edge;wing_up` | 所选边界名称用分号连接；空列表保存为空；选项来自工程边界及已保存名称 |
| 当前界面无对应控件 | — | `RefenceParametersData/RESULT_INTERVAL` | `100` | 是否与输出间隔有关？暂保留 |
| 当前界面无对应控件 | — | `OutputSettingData/PLOT_FUNCTIONS` | `1;0;0;0;0;0;1;0;1;0` | 与变量选择的关系待确认；暂保留 |
| 当前界面无对应控件 | — | `OutputSettingData/ISFORCE`、`ISWALL`、`ISRESULT` | 均为 `1` | 是否需要与间隔或变量选择联动？暂保留 |

### 输出变量位置表（顺序已确认）

位置从 1 开始，对应分号分隔列表。两组均为 17 项；选中为 1，未选中为 0，已接入文件读写。

| 位置 | 流场变量（VOLUME） | 内部选项值 | 壁面变量（BOUNDARY） | 内部选项值 |
| --- | --- | --- | --- | --- |
| 1 | 压力 | `pressure` | 压力 | `pressure` |
| 2 | 密度 | `density` | 密度 | `density` |
| 3 | 速度u | `velocity-u` | 速度u | `velocity-u` |
| 4 | 速度v | `velocity-v` | 速度v | `velocity-v` |
| 5 | 速度w | `velocity-w` | 速度w | `velocity-w` |
| 6 | 温度 | `temperature` | 温度 | `temperature` |
| 7 | 马赫数 | `mach` | 马赫数 | `mach` |
| 8 | 节点状态 | `node-status` | 节点状态 | `node-status` |
| 9 | 部件编号 | `component-id` | 部件编号 | `component-id` |
| 10 | 初始化状态 | `initialization-status` | 初始化状态 | `initialization-status` |
| 11 | 涡量值 | `vorticity-magnitude` | 热流 | `heat-flux` |
| 12 | x方向涡量 | `vorticity-x` | x方向摩擦力 | `friction-x` |
| 13 | y方向涡量 | `vorticity-y` | y方向摩擦力 | `friction-y` |
| 14 | z方向涡量 | `vorticity-z` | z方向摩擦力 | `friction-z` |
| 15 | 总温 | `total-temperature` | 摩擦力值 | `friction-magnitude` |
| 16 | 总压 | `total-pressure` | 总温 | `total-temperature` |
| 17 | Q准则涡量 | `q-criterion` | 总压 | `total-pressure` |
