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

find_package(Qt5 5.12 REQUIRED COMPONENTS Core)
if(AEROSIM_BUILD_GUI)
    find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets)
endif()

add_subdirectory(contracts)
add_subdirectory(src/mock)
add_subdirectory(src/application)

add_executable(aerosim_cli src/cli_main.cpp)
target_link_libraries(aerosim_cli PRIVATE aerosim_application aerosim_mock Qt5::Core)

if(AEROSIM_BUILD_GUI)
    add_subdirectory(src/ui_mock)
    add_subdirectory(src/infrastructure/demo)
    add_executable(aerosim_gui src/main.cpp)
    target_link_libraries(aerosim_gui PRIVATE aerosim_application aerosim_mock aerosim_ui_mock aerosim_postprocess_demo_launcher Qt5::Widgets)
endif()

if(AEROSIM_BUILD_TESTS)
    enable_testing()
    add_test(NAME cli_mock_roundtrip COMMAND aerosim_cli --mock-once)
    if(AEROSIM_BUILD_GUI)
        add_test(NAME gui_mock_smoke COMMAND aerosim_gui --smoke)
        set_tests_properties(gui_mock_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()

# B/C modules add their own subdirectories and dependencies when delivered.
# VTK 8.2 and SARibbon are intentionally not linked by this A-owned placeholder.
