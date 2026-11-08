cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AeroSimWorkflow LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_AUTOMOC ON)
    if(MSVC)
        add_compile_options(/utf-8)
    endif()
    option(AEROSIM_BUILD_TESTS "Build workflow tests" ON)
    if(AEROSIM_BUILD_TESTS)
        enable_testing()
    endif()
    find_package(Qt5 5.12.12 EXACT REQUIRED COMPONENTS Core)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../contracts"
        "${CMAKE_CURRENT_BINARY_DIR}/contracts")
endif()

add_library(aerosim_workflow STATIC
    src/WorkflowDefinition.cpp
    src/ValidationStampMatcher.cpp
    src/DependencyEvaluator.cpp
    src/ActionAuthorizer.cpp
    src/WorkflowStateDeriver.cpp
    src/WorkflowService.cpp
    src/ValidationCoordinator.cpp
    src/RegisterWorkflowMetaTypes.cpp
    src/WorkflowSnapshot.cpp
    include/aerosim/workflow/WorkflowDefinition.h
    include/aerosim/workflow/WorkflowFacts.h
    include/aerosim/workflow/ValidationStampMatcher.h
    include/aerosim/workflow/DependencyEvaluator.h
    include/aerosim/workflow/ActionAuthorizer.h
    include/aerosim/workflow/WorkflowStateDeriver.h
    include/aerosim/workflow/WorkflowService.h)
target_sources(aerosim_workflow PRIVATE
    include/aerosim/workflow/ValidationCoordinator.h
    include/aerosim/workflow/ValidationTypes.h)
target_sources(aerosim_workflow PRIVATE
    include/aerosim/workflow/WorkflowSnapshot.h)
add_library(aerosim::workflow ALIAS aerosim_workflow)
target_include_directories(aerosim_workflow
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(aerosim_workflow
    PUBLIC aerosim::contracts Qt5::Core)

if(MSVC)
    target_compile_options(aerosim_workflow PRIVATE /utf-8)
endif()

if(AEROSIM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
