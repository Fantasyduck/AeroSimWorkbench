add_library(aerosim_application STATIC ApplicationController.cpp)
add_library(aerosim::application ALIAS aerosim_application)
target_include_directories(aerosim_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aerosim_application PUBLIC aerosim_contracts Qt5::Core)

add_executable(aerosim_cli cli_main.cpp)
target_link_libraries(aerosim_cli PRIVATE aerosim_application aerosim_mock Qt5::Core)
set_target_properties(aerosim_cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(AEROSIM_BUILD_GUI)
    # A desktop GUI must not allocate a second console window on Windows.
    add_executable(aerosim_gui WIN32 gui_main.cpp)
    target_link_libraries(aerosim_gui PRIVATE
        aerosim_application aerosim_mock aerosim_ui Qt5::Widgets)
    # The monitor page owns an abstract MonitorCurveWidget slot. Filling it here
    # is the assembly layer's job, and only this executable has the chart stack
    # linked in, so the hook is compiled in conditionally rather than always.
    if(TARGET aerosim_monitor_curve)
        target_link_libraries(aerosim_gui PRIVATE aerosim_monitor_curve)
        target_compile_definitions(aerosim_gui PRIVATE AEROSIM_HAS_MONITOR_CURVES=1)
    endif()
    # The two result pages ("结果数据表" / "结果汇总表") take their reader and their
    # curve view through abstract ports. Filling those slots is the assembly
    # layer's job for the same reason as the monitor curve slot above: only this
    # executable has the monitor reader and Qwt linked in.
    if(TARGET aerosim_intelligent_source)
        target_link_libraries(aerosim_gui PRIVATE aerosim_intelligent_source)
        target_compile_definitions(aerosim_gui PRIVATE AEROSIM_HAS_RESULT_SOURCE=1)
    endif()
    if(TARGET aerosim_intelligent_curve)
        target_link_libraries(aerosim_gui PRIVATE aerosim_intelligent_curve)
        target_compile_definitions(aerosim_gui PRIVATE AEROSIM_HAS_RESULT_CURVES=1)
    endif()
    # Keep the application-level injection point. The scene target is absent
    # while visualization is only a skeleton, and is linked again as soon as
    # the new scene module provides it.
    if(TARGET aerosim_visualization_scene)
        target_link_libraries(aerosim_gui PRIVATE aerosim_visualization_scene)
        target_compile_definitions(aerosim_gui PRIVATE AEROSIM_HAS_VTK_VIEWER=1)
    endif()
    set_target_properties(aerosim_gui PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    # The standalone workbench has its own EXE and deployment directory so a
    # running main window does not lock its binary or its Qt runtime files.
    add_executable(aerosim_postprocess WIN32 gui_main.cpp)
    target_link_libraries(aerosim_postprocess PRIVATE
        aerosim_application aerosim_mock aerosim_ui Qt5::Widgets)
    target_compile_definitions(aerosim_postprocess PRIVATE AEROSIM_POSTPROCESS_STANDALONE=1)
    if(TARGET aerosim_visualization_scene)
        target_link_libraries(aerosim_postprocess PRIVATE aerosim_visualization_scene)
        target_compile_definitions(aerosim_postprocess PRIVATE AEROSIM_HAS_VTK_VIEWER=1)
    endif()
    set_target_properties(aerosim_postprocess PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/postprocess")
endif()
