cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AeroSimUI LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_AUTOMOC ON)
    if(MSVC)
        add_compile_options(/utf-8)
    endif()
    option(AEROSIM_BUILD_TESTS "Build UI integration tests" ON)
    if(AEROSIM_BUILD_TESTS)
        enable_testing()
    endif()
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core Widgets)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../contracts" contracts)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/SARibbonDependency.cmake")
if(NOT TARGET aerosim_configuration)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../configuration" configuration)
endif()
find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Svg)
add_subdirectory(monitor)
# The two result pages under 智能数据处理. Created before aerosim_ui so the
# window can link them; their concrete reader and curve view stay OPTIONAL and
# only exist when the visualization module was configured.
add_subdirectory(intelligent)
add_library(aerosim_ui STATIC
    src/ParameterTableCells.h
    src/RadioChoice.cpp include/aerosim/ui/RadioChoice.h
    resources/PostProcessIcons.qrc
    src/CalculationTaskPage.cpp include/aerosim/ui/CalculationTaskPage.h
    src/CalculationTaskInputDialog.cpp include/aerosim/ui/CalculationTaskInputDialog.h
    src/UiLayoutConfig.cpp include/aerosim/ui/UiLayoutConfig.h
    src/MainWindow.cpp include/aerosim/ui/MainWindow.h resources/RibbonIcons.qrc
    src/MeshManagementPage.cpp include/aerosim/ui/MeshManagementPage.h
    src/TrajectorySettingsPage.cpp include/aerosim/ui/TrajectorySettingsPage.h
    src/StartupLandingPage.cpp include/aerosim/ui/StartupLandingPage.h
    src/GoverningEquationsPanel.cpp include/aerosim/ui/GoverningEquationsPanel.h
    src/BoundaryConditionsPanel.cpp include/aerosim/ui/BoundaryConditionsPanel.h
    src/FarfieldParametersPanel.cpp include/aerosim/ui/FarfieldParametersPanel.h
    src/MeshMotionPanel.cpp include/aerosim/ui/MeshMotionPanel.h
    src/ForcedMotionDialog.cpp include/aerosim/ui/ForcedMotionDialog.h
    src/FixedAxisMotionDialog.cpp include/aerosim/ui/FixedAxisMotionDialog.h
    src/SixDofMotionDialog.cpp include/aerosim/ui/SixDofMotionDialog.h
    src/ReferenceValuesPanel.cpp include/aerosim/ui/ReferenceValuesPanel.h
    src/CalculationControlPanel.cpp include/aerosim/ui/CalculationControlPanel.h
    src/SolutionMethodPanel.cpp include/aerosim/ui/SolutionMethodPanel.h
    src/OutputControlPanel.cpp include/aerosim/ui/OutputControlPanel.h
    src/InitializationPanel.cpp include/aerosim/ui/InitializationPanel.h
    src/postprocess/PostProcessWorkbenchWindow.cpp
    include/aerosim/ui/postprocess/PostProcessWorkbenchWindow.h)
set_target_properties(aerosim_ui PROPERTIES AUTORCC ON)
add_library(aerosim::ui ALIAS aerosim_ui)
target_include_directories(aerosim_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(aerosim_ui PUBLIC aerosim_contracts aerosim_monitor_ui
    aerosim_configuration
    aerosim_intelligent_data Qt5::Widgets Qt5::Svg SARibbonBar::SARibbonBar)
if(MSVC)
    # Keep Chinese UI source literals UTF-8 when this module is configured as
    # part of the top-level project or built on its own.
    target_compile_options(aerosim_ui PRIVATE /utf-8)
endif()
if(AEROSIM_BUILD_TESTS)
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Test)
    add_executable(aerosim_xcf_ui_test tests/XcfProjectUiTest.cpp)
    target_link_libraries(aerosim_xcf_ui_test PRIVATE aerosim_ui)
    add_test(NAME xcf_project_ui COMMAND aerosim_xcf_ui_test)
    # Exercise the real modal Save/Cancel buttons. This bundled Qt 5.12 Windows
    # offscreen plugin crashes while painting QMessageBox (native plugin works).
    if(WIN32)
        set_tests_properties(xcf_project_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows")
    else()
        set_tests_properties(xcf_project_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
    add_executable(aerosim_parameter_layout_test tests/ParameterLayoutRegression.cpp)
    target_link_libraries(aerosim_parameter_layout_test PRIVATE aerosim_ui)
    add_test(NAME parameter_layout_regression COMMAND aerosim_parameter_layout_test)
    set_tests_properties(parameter_layout_regression PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(aerosim_ui_layout_framework_test tests/UiLayoutFrameworkTest.cpp)
    target_link_libraries(aerosim_ui_layout_framework_test PRIVATE aerosim_ui)
    add_test(NAME ui_layout_framework COMMAND aerosim_ui_layout_framework_test)
    set_tests_properties(ui_layout_framework PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(aerosim_postprocess_ui_smoke tests/postprocess/PostProcessWorkbenchWindowSmoke.cpp)
    target_link_libraries(aerosim_postprocess_ui_smoke PRIVATE aerosim_ui)
    add_test(NAME postprocess_ui_smoke COMMAND aerosim_postprocess_ui_smoke)
    set_tests_properties(postprocess_ui_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(aerosim_ui_test tests/MainWindowTest.cpp)
    target_link_libraries(aerosim_ui_test PRIVATE aerosim_ui)
    if(TARGET aerosim_calculation_monitor)
        target_link_libraries(aerosim_ui_test PRIVATE aerosim_calculation_monitor)
        target_compile_definitions(aerosim_ui_test PRIVATE AEROSIM_HAS_CALCULATION_MONITOR=1)
    endif()
    add_test(NAME ui_window_integration COMMAND aerosim_ui_test)
    set_tests_properties(ui_window_integration PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(aerosim_startup_landing_test tests/StartupLandingPageTest.cpp)
    target_link_libraries(aerosim_startup_landing_test PRIVATE aerosim_ui)
    add_test(NAME startup_landing_ui COMMAND aerosim_startup_landing_test)
    set_tests_properties(startup_landing_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
