include_guard(GLOBAL)

set(AEROSIM_EIGEN_ROOT "" CACHE PATH
    "Include root holding <eigen/Dense> or <Eigen/Dense>")
find_package(Eigen3 QUIET)
add_library(aerosim_eigen INTERFACE)
if(TARGET Eigen3::Eigen)
    target_link_libraries(aerosim_eigen INTERFACE Eigen3::Eigen)
    set(eigen_dense_header "Eigen/Dense")
else()
    find_path(AEROSIM_EIGEN_INCLUDE_DIR
        NAMES eigen/Dense Eigen/Dense
        PATHS "${AEROSIM_EIGEN_ROOT}"
              "${AEROSIM_EIGEN_ROOT}/eigen"
              "${AEROSIM_EIGEN_ROOT}/include"
              "${AEROSIM_EIGEN_ROOT}/include/eigen3"
        NO_DEFAULT_PATH)
    if(NOT AEROSIM_EIGEN_INCLUDE_DIR)
        message(FATAL_ERROR
            "Eigen 3 was not found. Set AEROSIM_EIGEN_ROOT to a directory containing "
            "Eigen/Dense or eigen/Dense. A compatible VTK SDK's vtkeigen directory "
            "can also be used; otherwise obtain Eigen 3 from "
            "https://gitlab.com/libeigen/eigen/-/releases.")
    endif()
    target_include_directories(aerosim_eigen INTERFACE "${AEROSIM_EIGEN_INCLUDE_DIR}")
    if(EXISTS "${AEROSIM_EIGEN_INCLUDE_DIR}/Eigen/Dense")
        set(eigen_dense_header "Eigen/Dense")
    else()
        set(eigen_dense_header "eigen/Dense")
    endif()
endif()
target_compile_definitions(aerosim_eigen INTERFACE
    "AEROSIM_EIGEN_DENSE_HEADER=\"${eigen_dense_header}\"")
