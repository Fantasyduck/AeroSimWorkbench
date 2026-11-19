include_guard(GLOBAL)

get_filename_component(eigen_project_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(AEROSIM_EIGEN_ROOT "" CACHE STRING
    "Eigen root, absolute or relative to the AeroSimWorkbench source root")
set_property(CACHE AEROSIM_EIGEN_ROOT PROPERTY TYPE STRING)
find_package(Eigen3 QUIET)
add_library(aerosim_eigen INTERFACE)
if(TARGET Eigen3::Eigen)
    target_link_libraries(aerosim_eigen INTERFACE Eigen3::Eigen)
    set(eigen_dense_header "Eigen/Dense")
else()
    set(eigen_search_roots "${AEROSIM_EIGEN_ROOT}")
    # Keep compatibility with callers that configured only the include directory.
    if(NOT AEROSIM_EIGEN_ROOT)
        list(APPEND eigen_search_roots "${AEROSIM_EIGEN_INCLUDE_DIR}")
    endif()
    list(APPEND eigen_search_roots
        ".tools/vtk-src/VTK-8.2.0/ThirdParty/eigen/vtkeigen"
        "../.tools/vtk-src/VTK-8.2.0/ThirdParty/eigen/vtkeigen"
        "third_party/VTK/ThirdParty/eigen/vtkeigen"
        "third_party/eigen")
    # A VTK source tree or installation can also supply its bundled headers.
    foreach(vtk_root IN ITEMS "${VTK_SOURCE_DIR}" "${VTK_DIR}" "${AEROSIM_VTK_DIR}")
        if(vtk_root STREQUAL "" OR vtk_root MATCHES "-NOTFOUND$")
            continue()
        endif()
        get_filename_component(vtk_prefix "${vtk_root}" ABSOLUTE BASE_DIR "${eigen_project_root}")
        foreach(level RANGE 0 3)
            list(APPEND eigen_search_roots
                "${vtk_prefix}/ThirdParty/eigen/vtkeigen"
                "${vtk_prefix}/include/vtk-8.2/vtkeigen")
            get_filename_component(vtk_prefix "${vtk_prefix}/.." ABSOLUTE)
        endforeach()
    endforeach()

    set(eigen_include_dir "")
    foreach(root IN LISTS eigen_search_roots)
        if(root STREQUAL "" OR root MATCHES "-NOTFOUND$")
            continue()
        endif()
        get_filename_component(root_path "${root}" ABSOLUTE BASE_DIR "${eigen_project_root}")
        foreach(suffix IN ITEMS "." "eigen" "include" "include/eigen3")
            get_filename_component(include_path "${root_path}/${suffix}" ABSOLUTE)
            if(EXISTS "${include_path}/Eigen/Dense" OR EXISTS "${include_path}/eigen/Dense")
                set(eigen_include_dir "${include_path}")
                break()
            endif()
        endforeach()
        if(eigen_include_dir)
            # Derive absolute compiler paths anew on each configuration.
            file(RELATIVE_PATH relative_root "${eigen_project_root}" "${root_path}")
            file(RELATIVE_PATH relative_include "${eigen_project_root}" "${eigen_include_dir}")
            set(AEROSIM_EIGEN_ROOT "${relative_root}" CACHE STRING
                "Eigen root relative to the AeroSimWorkbench source root" FORCE)
            set(AEROSIM_EIGEN_INCLUDE_DIR "${relative_include}" CACHE STRING
                "Eigen include directory relative to the AeroSimWorkbench source root" FORCE)
            break()
        endif()
    endforeach()
    if(NOT eigen_include_dir)
        message(FATAL_ERROR
            "Eigen 3 was not found in the project dependency directories or configured VTK. "
            "Set AEROSIM_EIGEN_ROOT to a directory containing Eigen/Dense or eigen/Dense, "
            "relative to ${eigen_project_root}. A compatible VTK SDK's vtkeigen directory "
            "can also be used; otherwise obtain Eigen 3 from "
            "https://gitlab.com/libeigen/eigen/-/releases.")
    endif()
    message(STATUS "Eigen headers: ${AEROSIM_EIGEN_INCLUDE_DIR} (relative to the source root)")
    target_include_directories(aerosim_eigen INTERFACE "${eigen_include_dir}")
    if(EXISTS "${eigen_include_dir}/Eigen/Dense")
        set(eigen_dense_header "Eigen/Dense")
    else()
        set(eigen_dense_header "eigen/Dense")
    endif()
endif()
target_compile_definitions(aerosim_eigen INTERFACE
    "AEROSIM_EIGEN_DENSE_HEADER=\"${eigen_dense_header}\"")
