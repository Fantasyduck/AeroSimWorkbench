cmake_minimum_required(VERSION 3.16)
project(AeroSimWorkbench VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()

option(AEROSIM_BUILD_GUI "Build the temporary Qt Widgets GUI shell" ON)
option(AEROSIM_BUILD_TESTS "Build and register skeleton smoke tests" ON)

option(AEROSIM_BUILD_STACK_PROBE "Build the optional Qt/VTK/SARibbon compatibility probe" OFF)

find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core)
if(AEROSIM_BUILD_GUI)
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Widgets)
endif()

add_subdirectory(contracts)
add_subdirectory(mocks/services)

if(AEROSIM_BUILD_GUI)
    add_subdirectory(mocks/ui)
    add_subdirectory(demos/postprocess/launcher)
endif()
add_subdirectory(app)

if(AEROSIM_BUILD_TESTS)
    enable_testing()
    add_test(NAME cli_mock_roundtrip COMMAND aerosim_cli --mock-once)
    if(AEROSIM_BUILD_GUI)
        add_test(NAME gui_mock_smoke COMMAND aerosim_gui --smoke)
        set_tests_properties(gui_mock_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME postprocess_fixture_check
            COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/postprocess/generate.py"
                    --check)
    endif()
endif()

if(AEROSIM_BUILD_STACK_PROBE)
    add_subdirectory(cmake/stack_probe)
endif()

# B-G modules add their own targets and dependencies under modules/ when delivered.
# VTK 8.2 and SARibbon are intentionally not linked by this A-owned placeholder.
