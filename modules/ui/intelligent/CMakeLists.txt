cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AeroSimIntelligentData LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_AUTOMOC ON)
    if(MSVC)
        add_compile_options(/utf-8)
    endif()
    option(AEROSIM_BUILD_TESTS "Build intelligent data page tests" ON)
    if(AEROSIM_BUILD_TESTS)
        enable_testing()
    endif()
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core Widgets)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../../contracts" contracts)
endif()

add_library(aerosim_intelligent_data STATIC
    ../src/intelligent/ResultDataTablePage.cpp
    ../src/intelligent/ResultSummaryTablePage.cpp
    ../include/aerosim/ui/intelligent/ResultDataSource.h
    ../include/aerosim/ui/intelligent/ResultDataTablePage.h
    ../include/aerosim/ui/intelligent/ResultSummaryTablePage.h)
target_include_directories(aerosim_intelligent_data PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../include")
target_link_libraries(aerosim_intelligent_data PUBLIC aerosim_contracts Qt5::Widgets)
if(MSVC)
    target_compile_options(aerosim_intelligent_data PRIVATE /utf-8)
endif()

# The concrete reader needs the monitor data/analysis layers, and those only
# exist when the visualization module was configured. The pages above keep
# talking to the abstract ResultDataSource, so turning visualization off still
# builds and tests the workbench -- it just cannot open a file.
if(TARGET aerosim_monitor_analysis)
    add_library(aerosim_intelligent_source STATIC
        ../src/intelligent/MonitorResultDataSource.cpp
        ../include/aerosim/ui/intelligent/MonitorResultDataSource.h)
    target_include_directories(aerosim_intelligent_source PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../include")
    target_link_libraries(aerosim_intelligent_source PUBLIC aerosim_intelligent_data aerosim_monitor_analysis Qt5::Widgets)
    if(MSVC)
        target_compile_options(aerosim_intelligent_source PRIVATE /utf-8)
    endif()
endif()

# The curve half needs the chart widget, which needs Qwt. Same conditional shape
# as the monitor curve adapter: no charting library, no curve view, and the page
# keeps its hint label instead.
if(TARGET aerosim_monitor_charts)
    add_library(aerosim_intelligent_curve STATIC
        ../src/intelligent/QwtResultCurveView.cpp
        ../include/aerosim/ui/intelligent/QwtResultCurveView.h)
    target_include_directories(aerosim_intelligent_curve PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../include")
    target_link_libraries(aerosim_intelligent_curve PUBLIC aerosim_intelligent_data aerosim_monitor_charts Qt5::Widgets)
    if(MSVC)
        target_compile_options(aerosim_intelligent_curve PRIVATE /utf-8)
    endif()
endif()

if(AEROSIM_BUILD_TESTS)
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Test)
    add_executable(aerosim_intelligent_data_test tests/ResultDataPagesTest.cpp)
    target_link_libraries(aerosim_intelligent_data_test PRIVATE aerosim_intelligent_data Qt5::Widgets Qt5::Test)
    set_target_properties(aerosim_intelligent_data_test PROPERTIES AUTOMOC ON)
    if(MSVC)
        target_compile_options(aerosim_intelligent_data_test PRIVATE /utf-8)
    endif()
    add_test(NAME intelligent_data_pages COMMAND aerosim_intelligent_data_test)
    set_tests_properties(intelligent_data_pages PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(AEROSIM_BUILD_TESTS AND TARGET aerosim_intelligent_source AND TARGET aerosim_intelligent_curve)
    add_executable(aerosim_result_adapters_test tests/ResultAdaptersTest.cpp)
    target_link_libraries(aerosim_result_adapters_test PRIVATE
        aerosim_intelligent_source aerosim_intelligent_curve Qt5::Test)
    target_compile_definitions(aerosim_result_adapters_test PRIVATE
        AEROSIM_RESULT_FIXTURE_DIR="${CMAKE_CURRENT_LIST_DIR}/../../../tests/fixtures/postprocess")
    set_target_properties(aerosim_result_adapters_test PROPERTIES AUTOMOC ON)
    if(MSVC)
        target_compile_options(aerosim_result_adapters_test PRIVATE /utf-8)
    endif()
    add_test(NAME intelligent_result_adapters COMMAND aerosim_result_adapters_test)
    set_tests_properties(intelligent_result_adapters PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
