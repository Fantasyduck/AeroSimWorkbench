cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AeroSimVisualization LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    if(MSVC)
        add_compile_options(/utf-8)
    endif()
endif()

# These options are declared outside the standalone-only block above. When this
# directory is added as a subproject they used to be undefined, so the chart
# layer - the whole point of enabling visualization - was silently skipped and
# the caller was left wondering why no curve widget existed. A cache option set
# by the parent wins; the default only applies where nothing set it.
option(AEROSIM_BUILD_TESTS "Build the visualization module unit tests" ON)
option(AEROSIM_BUILD_CHARTS "Build the QtWidgets chart view target" ON)

find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core)
if(AEROSIM_BUILD_CHARTS)
    # Qwt also needs Svg (symbols), PrintSupport and Concurrent (raster items),
    # so those components are requested here rather than inside charts/.
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS
        Widgets Svg PrintSupport Concurrent)
endif()

# data/ holds the file readers, analysis/ the 2-D series model built on top of
# them. Both stay usable in headless builds; only charts/ needs a widget library.
add_subdirectory(data)
add_subdirectory(analysis)

if(AEROSIM_BUILD_CHARTS)
    add_subdirectory(charts)
endif()

if(AEROSIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# scene/ is entered from the top-level CMakeLists instead of here: it holds both
# viewers (the preprocessing mesh viewport and the post-process workbench
# viewer) and must be added exactly once when either option is on. Adding it
# from this file as well would add the same directory twice; it also cannot
# stand alone because the workbench viewer links modules/ui.
