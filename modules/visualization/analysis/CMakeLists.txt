cmake_minimum_required(VERSION 3.16)

# Eigen supplies the least-squares solve behind polygon fits. It is header-only,
# so nothing is linked: the only thing the build needs is an include directory.
#
# The header spelling differs between installations, which is exactly what
# MonitorEigen.h probes for, so both layouts are accepted here:
#   <eigen/Dense>  VTK ships Eigen flattened like this (ThirdParty/eigen/vtkeigen)
#   <Eigen/Dense>  a standard Eigen 3 install
#
# It is looked up in two ways, mirroring how charts/CMakeLists.txt finds Qwt, so
# the build does not depend on one installation style and hard-codes no path:
#   1. find_package(Eigen3) - picks up Eigen3Config.cmake from a real install;
#   2. AEROSIM_EIGEN_ROOT - the include root the caller points at, which may be
#      an Eigen subtree of another dependency (e.g. VTK's ThirdParty/eigen).
set(AEROSIM_EIGEN_ROOT "" CACHE PATH
    "Include root holding <eigen/Dense> or <Eigen/Dense> (no absolute default is baked in)")

find_package(Eigen3 QUIET)
if(TARGET Eigen3::Eigen)
    set(AEROSIM_EIGEN_TARGET Eigen3::Eigen)
else()
    find_path(AEROSIM_EIGEN_INCLUDE_DIR
        NAMES eigen/Dense Eigen/Dense
        PATHS "${AEROSIM_EIGEN_ROOT}"
              "${AEROSIM_EIGEN_ROOT}/eigen"
              "${AEROSIM_EIGEN_ROOT}/include"
              "${AEROSIM_EIGEN_ROOT}/include/eigen3"
        NO_DEFAULT_PATH)
    if(AEROSIM_EIGEN_INCLUDE_DIR)
        add_library(aerosim_eigen INTERFACE)
        target_include_directories(aerosim_eigen INTERFACE "${AEROSIM_EIGEN_INCLUDE_DIR}")
        set(AEROSIM_EIGEN_TARGET aerosim_eigen)
    endif()
endif()

if(NOT AEROSIM_EIGEN_TARGET)
    message(FATAL_ERROR
        "Eigen 3 was not found. Install it, or configure with "
        "-DAEROSIM_EIGEN_ROOT=<include root containing eigen/Dense or Eigen/Dense>.")
endif()

# Two-dimensional view of a MonitorLog. Depends on the data target only, never
# on scene/ or on a widget library.
add_library(aerosim_monitor_analysis STATIC
    MonitorSeriesModel.cpp
    MonitorSeriesModel.h
    MonitorCurveFit.cpp
    MonitorCurveFit.h
    MonitorEigen.h)
add_library(aerosim::monitor_analysis ALIAS aerosim_monitor_analysis)
target_include_directories(aerosim_monitor_analysis PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(aerosim_monitor_analysis
    PUBLIC aerosim_monitor_data Qt5::Core
    PRIVATE ${AEROSIM_EIGEN_TARGET})
