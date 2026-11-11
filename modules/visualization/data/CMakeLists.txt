cmake_minimum_required(VERSION 3.16)

# Solver history reading. Links Qt5::Core only, so AEROSIM_BUILD_GUI=OFF builds
# still get the data path (see docs/PostProcessMonitorSeriesPlan.md, P0).
add_library(aerosim_monitor_data STATIC
    MonitorLogReader.cpp
    MonitorLogReader.h
    MonitorLiveSource.h
    MonitorLogAccumulator.cpp
    MonitorLogAccumulator.h
    MonitorFileTailSource.cpp
    MonitorFileTailSource.h)
add_library(aerosim::monitor_data ALIAS aerosim_monitor_data)
target_include_directories(aerosim_monitor_data PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(aerosim_monitor_data PUBLIC Qt5::Core)
