cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AeroSimMachineLearning LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    option(AEROSIM_BUILD_TESTS "Build machine learning tests" ON)
    if(AEROSIM_BUILD_TESTS)
        enable_testing()
    endif()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/EigenDependency.cmake")
add_library(aerosim_machine_learning STATIC
    src/RbfKrr.cpp
    include/aerosim/machine_learning/RbfKrr.h)
add_library(aerosim::machine_learning ALIAS aerosim_machine_learning)
target_compile_features(aerosim_machine_learning PUBLIC cxx_std_14)
target_include_directories(aerosim_machine_learning PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(aerosim_machine_learning PRIVATE aerosim_eigen)
set_target_properties(aerosim_machine_learning PROPERTIES AUTOMOC OFF)

if(AEROSIM_BUILD_TESTS)
    add_executable(aerosim_rbf_krr_test tests/RbfKrrTest.cpp)
    target_link_libraries(aerosim_rbf_krr_test PRIVATE aerosim::machine_learning)
    set_target_properties(aerosim_rbf_krr_test PROPERTIES AUTOMOC OFF)
    add_test(NAME machine_learning_rbf_krr COMMAND aerosim_rbf_krr_test)
endif()
