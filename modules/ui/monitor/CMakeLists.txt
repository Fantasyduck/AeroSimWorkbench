cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AeroSimMonitorUI LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    option(AEROSIM_BUILD_TESTS "Build monitoring UI tests" ON)
    if(AEROSIM_BUILD_TESTS)
        enable_testing()
    endif()
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core Widgets)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../../contracts" contracts)
endif()

add_library(aerosim_monitor_ui STATIC
    ../src/monitor/ComputeMonitorPage.cpp
    ../include/aerosim/ui/monitor/ComputeMonitorPage.h
    ../include/aerosim/ui/monitor/MonitorCurveWidget.h)
target_include_directories(aerosim_monitor_ui PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../include")
target_link_libraries(aerosim_monitor_ui PUBLIC aerosim_contracts Qt5::Widgets)
if(MSVC)
    target_compile_options(aerosim_monitor_ui PRIVATE /utf-8)
endif()
# The concrete curve widget needs the chart widget, and the chart widget needs
# Qwt. Visualization follows the GUI by default but can still be turned off
# (-DAEROSIM_BUILD_VISUALIZATION=OFF, or a build without the GUI), so this
# adapter is built only when that target is present.
# The monitor page itself keeps talking to the abstract MonitorCurveWidget, which
# is why the workbench can still be built and tested with no charting library.
if(TARGET aerosim_monitor_charts)
    add_library(aerosim_monitor_curve STATIC
        ../src/monitor/MonitorSampleCurveWidget.cpp
        ../include/aerosim/ui/monitor/MonitorSampleCurveWidget.h)
    target_include_directories(aerosim_monitor_curve PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../include")
    target_link_libraries(aerosim_monitor_curve PUBLIC aerosim_monitor_ui aerosim_monitor_charts Qt5::Widgets)
    if(MSVC)
        target_compile_options(aerosim_monitor_curve PRIVATE /utf-8)
    endif()
endif()

if(AEROSIM_BUILD_TESTS)
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Test)
    add_executable(aerosim_monitor_ui_test ../tests/monitor/ComputeMonitorPageTest.cpp)
    target_link_libraries(aerosim_monitor_ui_test PRIVATE aerosim_monitor_ui Qt5::Test)
    add_test(NAME compute_monitor_ui COMMAND aerosim_monitor_ui_test)
    set_tests_properties(compute_monitor_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
