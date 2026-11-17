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

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AeroSimVisualizationData LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    enable_testing()
endif()
find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core)

# CGNS adapter: the import path shared by the preprocessing mesh viewport and
# the post-process workbench. VTK and CGNS stay optional here, because this
# directory is also configured where neither is installed (AEROSIM_BUILD_GUI=OFF
# or a CGNS-less desktop build). A missing dependency is reported instead of
# being fatal; modules/visualization/scene falls back to probing CGNS itself
# when this target does not exist, and AEROSIM_MESH_HAS_CGNS stays undefined.
find_package(VTK 8.2.0 EXACT QUIET COMPONENTS vtkCommonCore vtkCommonDataModel)
set(AEROSIM_CGNS_ROOT "" CACHE PATH "CGNS installation or build prefix")
find_path(AEROSIM_CGNS_INCLUDE_DIR cgnslib.h
    HINTS "${AEROSIM_CGNS_ROOT}/include" "${AEROSIM_CGNS_ROOT}/src")
find_library(AEROSIM_CGNS_LIBRARY NAMES cgnsdll cgns
    HINTS "${AEROSIM_CGNS_ROOT}/lib" "${AEROSIM_CGNS_ROOT}/src/Debug")

if(VTK_FOUND AND AEROSIM_CGNS_INCLUDE_DIR AND AEROSIM_CGNS_LIBRARY)
    add_library(aerosim_visualization_data STATIC src/CgnsAdapter.cpp)
    target_include_directories(aerosim_visualization_data PUBLIC include ${VTK_INCLUDE_DIRS} PRIVATE "${AEROSIM_CGNS_INCLUDE_DIR}")
    target_link_libraries(aerosim_visualization_data PUBLIC Qt5::Core ${VTK_LIBRARIES} PRIVATE "${AEROSIM_CGNS_LIBRARY}")
    if(WIN32 AND AEROSIM_CGNS_LIBRARY MATCHES "cgnsdll")
        target_compile_definitions(aerosim_visualization_data PRIVATE USE_DLL)
    endif()
    if(MSVC)
        target_compile_options(aerosim_visualization_data PRIVATE /utf-8)
    endif()
    if(BUILD_TESTING OR AEROSIM_BUILD_TESTS OR CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        add_executable(aerosim_cgns_adapter_test ../tests/CgnsAdapterTest.cpp)
        target_include_directories(aerosim_cgns_adapter_test PRIVATE ${VTK_INCLUDE_DIRS})
        target_link_libraries(aerosim_cgns_adapter_test PRIVATE aerosim_visualization_data)
    endif()
    if(BUILD_TESTING OR AEROSIM_BUILD_TESTS OR CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        add_executable(aerosim_cgns_fixture_test ../tests/CgnsFixtureTest.cpp)
        target_include_directories(aerosim_cgns_fixture_test PRIVATE "${AEROSIM_CGNS_INCLUDE_DIR}")
        target_link_libraries(aerosim_cgns_fixture_test PRIVATE aerosim_visualization_data "${AEROSIM_CGNS_LIBRARY}")
        if(WIN32 AND AEROSIM_CGNS_LIBRARY MATCHES "cgnsdll")
            target_compile_definitions(aerosim_cgns_fixture_test PRIVATE USE_DLL)
        endif()
        add_test(NAME cgns_fixture_import COMMAND aerosim_cgns_fixture_test)
    endif()
else()
    message(STATUS "CGNS adapter target skipped: VTK 8.2.0 or CGNS not found "
        "(set AEROSIM_VTK_DIR / AEROSIM_CGNS_ROOT to enable it)")
endif()
