add_library(aerosim_mock STATIC
    MockWorkflowService.cpp
    MockWorkflowService.h
    MockTaskService.cpp
    MockTaskService.h
)
add_library(aerosim::mock ALIAS aerosim_mock)
target_include_directories(aerosim_mock PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aerosim_mock PUBLIC aerosim_contracts Qt5::Core)
