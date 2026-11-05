cmake_minimum_required(VERSION 3.16)

# Chart widget for the monitor series. This target is the only place that links a
# charting library: the model in analysis/ and the reader in data/ stay free of
# any plotting dependency, so the library choice remains reversible
# (docs/PostProcessMonitorSeriesPlan.md 8.2 - third-party licence review pending).
#
# The plotting library is Qwt. It is looked up in two ways so the build does not
# depend on one installation style:
#   1. find_package(Qwt) - a system or vendored package that exports Qwt::Plot;
#   2. AEROSIM_QWT_ROOT - a source tree or prefix the caller points at, used to
#      synthesise an imported target. No absolute path is hard-coded here.
set(AEROSIM_QWT_ROOT "" CACHE PATH
    "Root of a Qwt 7.x source tree or installation prefix (contains src/qwt or include/qwt)")

find_package(Qwt 7.5 QUIET)
if(TARGET Qwt::Plot)
    set(AEROSIM_QWT_TARGET Qwt::Plot)
elseif(TARGET qwt::plot)
    set(AEROSIM_QWT_TARGET qwt::plot)
else()
    find_library(AEROSIM_QWT_LIBRARY
        NAMES qwt qwt7 qwtplot
        PATHS "${AEROSIM_QWT_ROOT}" "${AEROSIM_QWT_ROOT}/lib"
        NO_DEFAULT_PATH)
    # A Qwt source tree keeps its headers in three places, and all three are
    # needed: qwt_global.h sits at the src/ root (not in a subdirectory),
    # qwt_plot.h in src/plot, qwt_scale_map.h and qwt_scale_engine.h in src/core.
    # Each is searched separately so that a missing one fails here instead of
    # surfacing as a compile error later.
    find_path(AEROSIM_QWT_INCLUDE_DIR
        NAMES qwt_plot.h
        PATHS "${AEROSIM_QWT_ROOT}/src/plot" "${AEROSIM_QWT_ROOT}/include/qwt"
        NO_DEFAULT_PATH)
    find_path(AEROSIM_QWT_CORE_INCLUDE_DIR
        NAMES qwt_scale_map.h
        PATHS "${AEROSIM_QWT_ROOT}/src/core" "${AEROSIM_QWT_ROOT}/include/qwt"
        NO_DEFAULT_PATH)
    find_path(AEROSIM_QWT_ROOT_INCLUDE_DIR
        NAMES qwt_global.h
        PATHS "${AEROSIM_QWT_ROOT}/src" "${AEROSIM_QWT_ROOT}/include/qwt"
        NO_DEFAULT_PATH)
    if(AEROSIM_QWT_LIBRARY AND AEROSIM_QWT_INCLUDE_DIR
       AND AEROSIM_QWT_CORE_INCLUDE_DIR AND AEROSIM_QWT_ROOT_INCLUDE_DIR)
        add_library(aerosim_qwt UNKNOWN IMPORTED)
        set_target_properties(aerosim_qwt PROPERTIES
            IMPORTED_LOCATION "${AEROSIM_QWT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${AEROSIM_QWT_INCLUDE_DIR};${AEROSIM_QWT_CORE_INCLUDE_DIR};${AEROSIM_QWT_ROOT_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "Qt5::Widgets;Qt5::Gui;Qt5::Core;Qt5::Svg;Qt5::PrintSupport;Qt5::Concurrent")
        set(AEROSIM_QWT_TARGET aerosim_qwt)
    endif()
endif()

if(NOT AEROSIM_QWT_TARGET)
    message(FATAL_ERROR
        "Qwt 7.5 was not found. Install it, or configure with -DAEROSIM_QWT_ROOT=<path to Qwt 7.x>.")
endif()

add_library(aerosim_monitor_charts STATIC
    MonitorChartView.cpp
    MonitorChartView.h)
add_library(aerosim::monitor_charts ALIAS aerosim_monitor_charts)
target_include_directories(aerosim_monitor_charts PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(aerosim_monitor_charts
    PUBLIC aerosim_monitor_analysis Qt5::Widgets ${AEROSIM_QWT_TARGET})
