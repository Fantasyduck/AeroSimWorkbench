cmake_minimum_required(VERSION 3.18)
project(AeroSimQwtDependency LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Qwt setup requires an MSVC x64 compiler")
endif()
find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport Concurrent OpenGL)
set(AEROSIM_QWT_SOURCE_DIR "" CACHE PATH "Qwt 7.5.2 source root")
set(QWT_VERSION_MAJOR 7)
set(QWT_VERSION_MINOR 5)
set(QWT_VERSION_PATCH 2)
set(QWT_VERSION "7.5.2")
configure_file("${AEROSIM_QWT_SOURCE_DIR}/src/qwt_version_info.h.in"
    "${AEROSIM_QWT_SOURCE_DIR}/src/qwt_version_info.h" @ONLY)
# The application uses the 2D library as one static archive.
file(GLOB qwt_sources CONFIGURE_DEPENDS
    "${AEROSIM_QWT_SOURCE_DIR}/src/core/*.cpp"
    "${AEROSIM_QWT_SOURCE_DIR}/src/plot/*.cpp")
file(GLOB qwt_headers CONFIGURE_DEPENDS
    "${AEROSIM_QWT_SOURCE_DIR}/src/core/*.h"
    "${AEROSIM_QWT_SOURCE_DIR}/src/plot/*.h")
add_library(qwt7 STATIC ${qwt_sources} ${qwt_headers})
target_include_directories(qwt7 PUBLIC "${AEROSIM_QWT_SOURCE_DIR}/src"
    "${AEROSIM_QWT_SOURCE_DIR}/src/core" "${AEROSIM_QWT_SOURCE_DIR}/src/plot")
target_link_libraries(qwt7 PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Svg
    Qt5::PrintSupport Qt5::Concurrent Qt5::OpenGL)
target_compile_options(qwt7 PRIVATE /utf-8 /bigobj /Zc:__cplusplus
    "/FI${CMAKE_CURRENT_SOURCE_DIR}/Qt512MsvcCompatibility.h")
set_source_files_properties("${AEROSIM_QWT_SOURCE_DIR}/src/core/qwt_simd_argminmax_avx2.cpp"
    PROPERTIES COMPILE_OPTIONS /arch:AVX2)
set_target_properties(qwt7 PROPERTIES DEBUG_POSTFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${AEROSIM_QWT_SOURCE_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${AEROSIM_QWT_SOURCE_DIR}/lib")

# Export the complete static dependency chain, including Qt OpenGL.
include(CMakePackageConfigHelpers)
set(qwt_package_dir "${AEROSIM_QWT_SOURCE_DIR}/lib/cmake/Qwt")
file(MAKE_DIRECTORY "${qwt_package_dir}")
export(TARGETS qwt7 FILE "${qwt_package_dir}/QwtTargets.cmake" NAMESPACE Qwt::)
file(WRITE "${qwt_package_dir}/QwtConfig.cmake"
    "include(CMakeFindDependencyMacro)\n"
    "find_dependency(Qt5 5.12.12 EXACT COMPONENTS Core Gui Widgets Svg PrintSupport Concurrent OpenGL)\n"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/QwtTargets.cmake\")\n"
    "if(NOT TARGET Qwt::Plot)\n    add_library(Qwt::Plot ALIAS Qwt::qwt7)\nendif()\n")
write_basic_package_version_file("${qwt_package_dir}/QwtConfigVersion.cmake"
    VERSION ${QWT_VERSION} COMPATIBILITY SameMajorVersion)
