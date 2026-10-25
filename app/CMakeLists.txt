add_library(aerosim_application STATIC ApplicationController.cpp)
add_library(aerosim::application ALIAS aerosim_application)
target_include_directories(aerosim_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aerosim_application PUBLIC aerosim_contracts Qt5::Core)

add_executable(aerosim_cli cli_main.cpp)
target_link_libraries(aerosim_cli PRIVATE aerosim_application aerosim_mock Qt5::Core)
set_target_properties(aerosim_cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(AEROSIM_BUILD_GUI)
    add_executable(aerosim_gui gui_main.cpp)
    target_link_libraries(aerosim_gui PRIVATE
        aerosim_application aerosim_mock aerosim_ui_mock Qt5::Widgets)
    set_target_properties(aerosim_gui PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
