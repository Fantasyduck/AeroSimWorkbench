set(SARIBBON_SOURCE_DIR "" CACHE PATH "SARibbon source checkout")
if(NOT EXISTS "${SARIBBON_SOURCE_DIR}/src/SARibbonBar/SARibbonBar.h")
    message(FATAL_ERROR "Set SARIBBON_SOURCE_DIR to a SARibbon source checkout")
endif()
file(READ "${SARIBBON_SOURCE_DIR}/CMakeLists.txt" _saribbon_cmake)
if(NOT _saribbon_cmake MATCHES "set\\(SARIBBON_VERSION_MAJOR[ \t]+2\\)" OR
   NOT _saribbon_cmake MATCHES "set\\(SARIBBON_VERSION_MINOR[ \t]+9\\)" OR
   NOT _saribbon_cmake MATCHES "set\\(SARIBBON_VERSION_PATCH[ \t]+[23]\\)")
    message(FATAL_ERROR "The UI requires SARibbon 2.9.3 source")
endif()


find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Widgets Svg)
# SARibbon 2.9.x expects the host application's Qt selection.
set(QT_VERSION_MAJOR 5)
set(QT_DIR "${Qt5_DIR}")
set(SARIBBON_BUILD_EXAMPLES OFF)
set(SARIBBON_BUILD_STATIC_LIBS ON)
set(SARIBBON_USE_FRAMELESS_LIB OFF)
set(SARIBBON_INSTALL_IN_CURRENT_DIR OFF)
set(BUILD_TESTS OFF)
# Honor the host options even when SARibbon resets policies with an older
# cmake_minimum_required (also applies to standalone modules/ui builds).
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
if(NOT TARGET SARibbonBar::SARibbonBar)
    add_subdirectory("${SARIBBON_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/saribbon" EXCLUDE_FROM_ALL)
endif()
