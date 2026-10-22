add_library(aerosim_postprocess_demo_launcher STATIC
    PostprocessDemoLauncher.cpp
    PostprocessDemoLauncher.h
)
add_library(aerosim::postprocess_demo_launcher ALIAS aerosim_postprocess_demo_launcher)
target_include_directories(aerosim_postprocess_demo_launcher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aerosim_postprocess_demo_launcher PUBLIC Qt5::Core)
