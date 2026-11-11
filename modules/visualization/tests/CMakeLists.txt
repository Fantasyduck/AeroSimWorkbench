cmake_minimum_required(VERSION 3.16)

# The fixture directory is derived from this file so the test also works when
# modules/visualization is configured as a standalone project.
set(AEROSIM_POSTPROCESS_FIXTURE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../../tests/fixtures/postprocess" CACHE PATH
    "Post-processing fixture directory")

# Headless: these tests link no widget library, so they also run with
# AEROSIM_BUILD_GUI=OFF.
add_executable(aerosim_monitor_log_reader_test MonitorLogReaderTest.cpp)
target_link_libraries(aerosim_monitor_log_reader_test PRIVATE aerosim_monitor_analysis)
add_test(NAME monitor_log_reader COMMAND aerosim_monitor_log_reader_test)
set_tests_properties(monitor_log_reader PROPERTIES
    ENVIRONMENT "AEROSIM_POSTPROCESS_FIXTURE_DIR=${AEROSIM_POSTPROCESS_FIXTURE_DIR}")

# The render test needs the chart widget, so it only exists when charts/ was
# built. It writes real PNGs and asserts on them: "draws something" is a
# separate claim from "computes the right points".
if(TARGET aerosim_monitor_charts)
    set(AEROSIM_MONITOR_RENDER_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/monitor_render" CACHE PATH
        "Where the monitor chart render test writes its PNGs")
    add_executable(aerosim_monitor_chart_render_test MonitorChartRenderTest.cpp)
    target_link_libraries(aerosim_monitor_chart_render_test PRIVATE aerosim_monitor_charts)
    add_test(NAME monitor_chart_render COMMAND aerosim_monitor_chart_render_test)
    set_tests_properties(monitor_chart_render PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;AEROSIM_POSTPROCESS_FIXTURE_DIR=${AEROSIM_POSTPROCESS_FIXTURE_DIR};AEROSIM_MONITOR_RENDER_DIR=${AEROSIM_MONITOR_RENDER_DIR}")
endif()
