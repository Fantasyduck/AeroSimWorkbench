include_guard(GLOBAL)

set(_AEROSIM_QT_DEPLOY_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/DeployQtRuntime.cmake")
set(AEROSIM_VTK_BIN "$ENV{AEROSIM_VTK_BIN}" CACHE PATH
    "Extra directory containing the matching VTK runtime DLLs")

function(aerosim_deploy_qt_runtime target)
    if(NOT WIN32)
        return()
    endif()

    get_target_property(qmake_executable Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(qt_bin_dir "${qmake_executable}" DIRECTORY)
    find_program(AEROSIM_WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${qt_bin_dir}" NO_DEFAULT_PATH)
    if(NOT AEROSIM_WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "The selected Qt SDK must provide bin/windeployqt.exe")
    endif()

    set(runtime_directories "")
    if(TARGET vtkCommonCore)
        list(APPEND runtime_directories "$<TARGET_FILE_DIR:vtkCommonCore>")
    endif()
    if(AEROSIM_VTK_BIN)
        list(APPEND runtime_directories "${AEROSIM_VTK_BIN}")
    endif()
    if(AEROSIM_CGNS_ROOT)
        list(APPEND runtime_directories "${AEROSIM_CGNS_ROOT}/bin"
            "${AEROSIM_CGNS_ROOT}/src/$<CONFIG>")
    endif()
    set(runtime_config_file "${CMAKE_CURRENT_BINARY_DIR}/${target}-runtime-$<CONFIG>.cmake")
    set(runtime_config "set(EXTRA_RUNTIME_DIRECTORIES [==[${runtime_directories}]==])\n")
    get_property(vtk_runtime_file GLOBAL PROPERTY AEROSIM_VTK_RUNTIME_FILE)
    if(vtk_runtime_file)
        string(APPEND runtime_config "include([==[${vtk_runtime_file}]==])\n")
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${vtk_runtime_file}")
    endif()
    file(GENERATE OUTPUT "${runtime_config_file}"
        CONTENT "${runtime_config}")
    get_filename_component(compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(AEROSIM_DUMPBIN_EXECUTABLE dumpbin HINTS "${compiler_directory}")

    set(deploy_command "${CMAKE_COMMAND}"
        "-DAPP_EXECUTABLE=$<TARGET_FILE:${target}>"
        "-DQT_BIN_DIR=${qt_bin_dir}"
        "-DWINDEPLOYQT_EXECUTABLE=${AEROSIM_WINDEPLOYQT_EXECUTABLE}"
        "-DBUILD_CONFIGURATION=$<CONFIG>"
        "-DRUNTIME_DIRECTORIES_FILE=${runtime_config_file}"
        "-DRUNTIME_DEPENDENCY_TOOL=${AEROSIM_DUMPBIN_EXECUTABLE}"
        -P "${_AEROSIM_QT_DEPLOY_SCRIPT}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${deploy_command}
        COMMENT "Deploying Qt and application runtime libraries for ${target}"
        VERBATIM)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
        "${_AEROSIM_QT_DEPLOY_SCRIPT}" "${runtime_config_file}")
    # Can also repair the runtime directory without relinking a running GUI.
    add_custom_target(${target}_deploy
        COMMAND ${deploy_command}
        DEPENDS ${target}
        COMMENT "Refreshing Qt runtime for ${target}"
        VERBATIM)
endfunction()
