# 派生对象 UI 核心设计

本设计参考 Tecplot 360 的常用交互，保留本工作台所需的核心功能；保存的是 UI 草稿，数据读取、拾取、计算和渲染由后续服务实现。字段及区域由对应数据源提供，未知单位和节点/单元位置不推断。

| 对象 | 输入与定义 | 其他设置 |
| --- | --- | --- |
| 云图 | 区域或对象、标量 | 配色、自动或手动值域、图例 |
| 切面 | 体区域；X/Y/Z 位置或原点及非零法向 | 表面/网格显示、可选着色字段 |
| 等值线 | 物面或切面及标量；指定一个或多个等值，或最小值、最大值和级数 | 线宽、单色或另一标量着色 |
| 等值面 | 体区域及标量；最多三个指定等值 | 不透明度、单色或另一标量着色 |
| 流线 | 速度矢量；体/表面、单点或线段种子、积分方向和限制 | 单色或标量着色 |
| 迹线 | 多时间步体数据和速度矢量；粒子路径或连续释放、种子及时间范围 | 连续释放的频率、时间积分步长、显示着色 |

体流线要求三维输入；线种子的两端不能重合。手动范围必须递增；数量、积分步长、线宽为正值。迹线结束时间晚于释放时间。视口拾取、时间边界与字段位置的真实校验依赖服务接入，当前不假装已经完成。

参考：[切面](https://tecplot.azureedge.net/products/360/current/help/users_manual/slices.html)、[等值线级别](https://tecplot.azureedge.net/products/360/2023r2/help/Topics/enter_contour_level_range.htm)、[等值面](https://tecplot.azureedge.net/products/360/current/help/users_manual/isosurf.html)、[流线](https://tecplot.azureedge.net/products/360/current/help/users_manual/streamtr.html)、[粒子路径及迹线](https://tecplot.azureedge.net/products/360/2023r2/help/Topics/cfda_particle_paths_and_streaklines.htm)。
