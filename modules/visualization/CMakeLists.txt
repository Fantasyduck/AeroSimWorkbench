cmake_minimum_required(VERSION 3.18)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AeroSimFeatures LANGUAGES CXX)
    option(AEROSIM_BUILD_TESTS "Build feature tests" ON)
    if(AEROSIM_BUILD_TESTS)
        enable_testing()
    endif()
endif()

find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core)
find_package(VTK 8.2.0 EXACT REQUIRED COMPONENTS
    vtkCommonCore vtkCommonDataModel vtkFiltersCore vtkFiltersGeneral
    vtkFiltersGeometry vtkIOXML vtkIOLegacy)
find_package(Threads REQUIRED)

add_library(aerosim_spatial_features STATIC
    src/features/FeatureProcessor.cpp
    src/features/FeatureExporter.cpp
    src/features/FeatureJob.cpp)
add_library(aerosim::spatial_features ALIAS aerosim_spatial_features)
target_include_directories(aerosim_spatial_features PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_include_directories(aerosim_spatial_features SYSTEM PUBLIC ${VTK_INCLUDE_DIRS})
target_link_libraries(aerosim_spatial_features PUBLIC Qt5::Core ${VTK_LIBRARIES} Threads::Threads)
set_target_properties(aerosim_spatial_features PROPERTIES
    CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF AUTOMOC OFF)
if(MSVC)
    target_compile_options(aerosim_spatial_features PRIVATE /utf-8)
    # Target machine only has VTK Debug. Never accidentally link its CRT to Release.
    target_compile_definitions(aerosim_spatial_features PRIVATE
        "$<$<NOT:$<CONFIG:Debug>>:AEROSIM_FEATURES_REQUIRES_DEBUG>")
endif()

if(AEROSIM_BUILD_TESTS)
    add_executable(aerosim_spatial_features_test tests/FeatureTests.cpp)
    target_link_libraries(aerosim_spatial_features_test PRIVATE aerosim_spatial_features)
    set_target_properties(aerosim_spatial_features_test PROPERTIES
        CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF AUTOMOC OFF)
    if(MSVC)
        target_compile_options(aerosim_spatial_features_test PRIVATE /utf-8)
    endif()
    get_filename_component(_fixture_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures/postprocess" ABSOLUTE)
    add_test(NAME spatial_features COMMAND aerosim_spatial_features_test "${_fixture_dir}")
    set_tests_properties(spatial_features PROPERTIES TIMEOUT 120)
endif()
