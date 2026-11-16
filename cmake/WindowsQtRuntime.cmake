include_guard(GLOBAL)

set(_AEROSIM_QT_DEPLOY_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/DeployQtRuntime.cmake")

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

    set(deploy_command "${CMAKE_COMMAND}"
        "-DAPP_EXECUTABLE=$<TARGET_FILE:${target}>"
        "-DQT_BIN_DIR=${qt_bin_dir}"
        "-DWINDEPLOYQT_EXECUTABLE=${AEROSIM_WINDEPLOYQT_EXECUTABLE}"
        "-DBUILD_CONFIGURATION=$<CONFIG>"
        -P "${_AEROSIM_QT_DEPLOY_SCRIPT}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${deploy_command}
        COMMENT "Deploying Qt runtime for ${target}"
        VERBATIM)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${_AEROSIM_QT_DEPLOY_SCRIPT}")
    # Can also repair the runtime directory without relinking a running GUI.
    add_custom_target(${target}_deploy
        COMMAND ${deploy_command}
        DEPENDS ${target}
        COMMENT "Refreshing Qt runtime for ${target}"
        VERBATIM)
endfunction()
