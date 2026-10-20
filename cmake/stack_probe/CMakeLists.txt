# Optional, explicit full-stack check. Ordinary targets do not require VTK or SARibbon.
set(SARIBBON_SOURCE_DIR "" CACHE PATH "SARibbon source checkout for stack probe")
if(NOT EXISTS "${SARIBBON_SOURCE_DIR}/src/SARibbonBar/SARibbonBar.h")
    message(FATAL_ERROR "Set SARIBBON_SOURCE_DIR to a SARibbon source checkout")
endif()
file(READ "${SARIBBON_SOURCE_DIR}/CMakeLists.txt" _saribbon_cmake)
if(NOT _saribbon_cmake MATCHES "set\\(SARIBBON_VERSION_MAJOR[ \t]+2\\)" OR
   NOT _saribbon_cmake MATCHES "set\\(SARIBBON_VERSION_MINOR[ \t]+9\\)" OR
   NOT _saribbon_cmake MATCHES "set\\(SARIBBON_VERSION_PATCH[ \t]+3\\)")
    message(FATAL_ERROR "The stack probe requires SARibbon 2.9.3 source")
endif()

find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Widgets OpenGL Svg)
find_package(VTK 8.2.0 EXACT REQUIRED COMPONENTS
    vtkCommonCore vtkGUISupportQt vtkRenderingCore vtkRenderingOpenGL2)

# SARibbon 2.9.x expects the host application's Qt selection.
set(QT_VERSION_MAJOR 5)
set(QT_DIR "${Qt5_DIR}")
set(SARIBBON_BUILD_EXAMPLES OFF)
set(SARIBBON_BUILD_STATIC_LIBS ON)
set(SARIBBON_USE_FRAMELESS_LIB OFF)
set(SARIBBON_INSTALL_IN_CURRENT_DIR OFF)
set(BUILD_TESTS OFF)
add_subdirectory("${SARIBBON_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/saribbon" EXCLUDE_FROM_ALL)

include("${VTK_USE_FILE}")
add_executable(aerosim_stack_probe StackProbe.cpp)
target_link_libraries(aerosim_stack_probe PRIVATE
    Qt5::Widgets Qt5::OpenGL SARibbonBar::SARibbonBar ${VTK_LIBRARIES})
if(MSVC)
    get_target_property(_vtk_configs vtkGUISupportQt IMPORTED_CONFIGURATIONS)
    if("DEBUG" IN_LIST _vtk_configs AND NOT "RELEASE" IN_LIST _vtk_configs)
        message(STATUS "VTK vtkGUISupportQt provides Debug only; Release stack probe will fail at compile time")
        target_compile_definitions(aerosim_stack_probe PRIVATE
            "$<$<NOT:$<CONFIG:Debug>>:AEROSIM_VTK_DEBUG_ONLY>")
    endif()
endif()
if(AEROSIM_BUILD_TESTS)
    add_test(NAME stack_probe COMMAND aerosim_stack_probe)
endif()
