cmake_minimum_required(VERSION 3.16)
project(AeroSimWorkbench VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()

option(AEROSIM_BUILD_GUI "Build the SARibbon main window" ON)
option(AEROSIM_BUILD_TESTS "Build and register skeleton smoke tests" ON)

option(AEROSIM_BUILD_STACK_PROBE "Build the optional Qt/VTK/SARibbon compatibility probe" OFF)
# Visualization is on by default: the monitor page gets its real curve widget
# only when charts/ is present, and a default build is supposed to show curves,
# not a placeholder. It follows AEROSIM_BUILD_GUI because the chart layer needs
# Qt5::Widgets - a headless configuration must not be forced to provide Qwt.
#
# Turning this on makes Qwt and Eigen required: configure fails with an
# actionable message rather than silently dropping the curve widget, so pass
# -DAEROSIM_QWT_ROOT and -DAEROSIM_EIGEN_ROOT (or have them installed) or
# explicitly set -DAEROSIM_BUILD_VISUALIZATION=OFF.
option(AEROSIM_BUILD_VISUALIZATION "Enable visualization/post-processing module targets" ${AEROSIM_BUILD_GUI})

find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core)
if(AEROSIM_BUILD_GUI)
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Widgets)
endif()

if(AEROSIM_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(contracts)
add_subdirectory(modules/workflow)
add_subdirectory(mocks/services)

# Visualization is configured before the UI so that modules/ui can see
# aerosim_monitor_charts and opt into the concrete curve widget. The UI never
# requires it: whenever visualization is off, the monitor page keeps its
# placeholder and the abstract MonitorCurveWidget injection point.
if(AEROSIM_BUILD_VISUALIZATION)
    add_subdirectory(modules/visualization)
endif()

if(AEROSIM_BUILD_GUI)
    add_subdirectory(modules/ui)
endif()

add_subdirectory(app)

if(AEROSIM_BUILD_TESTS)
    enable_testing()
    add_test(NAME cli_mock_roundtrip COMMAND aerosim_cli --mock-once)
    if(AEROSIM_BUILD_GUI)
        add_test(NAME gui_mock_smoke COMMAND aerosim_gui --smoke)
        set_tests_properties(gui_mock_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        add_test(NAME postprocess_gui_mock_smoke COMMAND aerosim_gui --postprocess --smoke)
        set_tests_properties(postprocess_gui_mock_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        add_test(NAME postprocess_standalone_mock_smoke COMMAND aerosim_postprocess --smoke)
        set_tests_properties(postprocess_standalone_mock_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
    add_test(NAME postprocess_fixture_check
        COMMAND "${CMAKE_COMMAND}"
                "-DPOSTPROCESS_FIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/postprocess"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/postprocess/VerifyFixtures.cmake")
endif()

if(AEROSIM_BUILD_STACK_PROBE)
    add_subdirectory(cmake/stack_probe)
endif()

# Other modules add their own targets and dependencies under modules/ when delivered.
