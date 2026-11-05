cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AeroSimVisualization LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    if(MSVC)
        add_compile_options(/utf-8)
    endif()
    option(AEROSIM_BUILD_TESTS "Build the visualization module unit tests" ON)
    option(AEROSIM_BUILD_CHARTS "Build the QtWidgets chart view target" ON)
    option(AEROSIM_BUILD_MONITOR_DEMO "Build the standalone monitor chart demo" ON)
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core)
    if(AEROSIM_BUILD_CHARTS)
        # Qwt also needs Svg (symbols), PrintSupport and Concurrent (raster items),
        # so those components are requested here rather than inside charts/.
        find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS
            Widgets Svg PrintSupport Concurrent)
    endif()
endif()

# data/ holds the file readers, analysis/ the 2-D series model built on top of
# them. Both stay usable in headless builds; only charts/ needs a widget library.
add_subdirectory(data)
add_subdirectory(analysis)

if(AEROSIM_BUILD_CHARTS)
    add_subdirectory(charts)
endif()

# The demo is a development aid over the three layers above; it is skipped when
# the chart widget itself was skipped.
if(AEROSIM_BUILD_CHARTS AND AEROSIM_BUILD_MONITOR_DEMO)
    add_subdirectory(demo)
endif()

if(AEROSIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
