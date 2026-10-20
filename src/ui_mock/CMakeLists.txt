add_library(aerosim_ui_mock STATIC MockWindow.cpp MockWindow.h)
add_library(aerosim::ui_mock ALIAS aerosim_ui_mock)
target_include_directories(aerosim_ui_mock PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/ui/include)
target_link_libraries(aerosim_ui_mock PUBLIC aerosim_contracts Qt5::Widgets)
