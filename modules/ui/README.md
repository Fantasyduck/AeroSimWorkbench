# 主界面

本模块包含 SARibbon MainWindow、工程树、WorkflowPanel、PropertyPanel、监控与二维曲线界面。MainWindow 实现 `aerosim/ui/IntegrationPorts.h` 中的 `IWidgetHost`，绑定工作流/运行等服务，并为可视化、日志控件提供插入位置。工作流规则和工程持久化不在本模块实现。
