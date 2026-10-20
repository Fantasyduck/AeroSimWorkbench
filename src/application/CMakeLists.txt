add_library(aerosim_application STATIC ApplicationController.cpp)
add_library(aerosim::application ALIAS aerosim_application)
target_include_directories(aerosim_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aerosim_application PUBLIC aerosim_contracts Qt5::Core)
