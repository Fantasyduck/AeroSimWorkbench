cmake_minimum_required(VERSION 3.16)

# Two-dimensional view of a MonitorLog. Depends on the data target only, never
# on scene/ or on a widget library.
add_library(aerosim_monitor_analysis STATIC
    MonitorSeriesModel.cpp
    MonitorSeriesModel.h
    MonitorCurveFit.cpp
    MonitorCurveFit.h)
add_library(aerosim::monitor_analysis ALIAS aerosim_monitor_analysis)
target_include_directories(aerosim_monitor_analysis PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(aerosim_monitor_analysis PUBLIC aerosim_monitor_data Qt5::Core)
