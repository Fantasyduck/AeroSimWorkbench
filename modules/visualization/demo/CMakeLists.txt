cmake_minimum_required(VERSION 3.16)

# The demo is a thin front end over data/, analysis/ and charts/. It links no
# plotting header of its own -- everything goes through MonitorChartView -- so it
# is also the smallest proof that the public API is usable from outside.
#
# It is a development aid, not a shipping window: it is only built when this
# module is configured on its own or when the caller explicitly asks for it, so
# the main application is never dragged into depending on it.
option(AEROSIM_BUILD_MONITOR_DEMO "Build the standalone monitor chart demo" ON)

if(AEROSIM_BUILD_MONITOR_DEMO)
    add_executable(aerosim_monitor_demo
        main.cpp
        MonitorChartDemoWindow.cpp
        MonitorChartDemoWindow.h)
    target_include_directories(aerosim_monitor_demo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(aerosim_monitor_demo PRIVATE aerosim_monitor_charts Qt5::Widgets)
endif()
