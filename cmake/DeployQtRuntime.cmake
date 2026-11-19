if(NOT EXISTS "${APP_EXECUTABLE}" OR NOT EXISTS "${WINDEPLOYQT_EXECUTABLE}")
    message(FATAL_ERROR "The application and the Qt deployment tool must exist")
endif()

if(BUILD_CONFIGURATION STREQUAL "Debug")
    set(deploy_configuration --debug)
    set(qt_opengl_dll Qt5OpenGLd.dll)
else()
    set(deploy_configuration --release)
    set(qt_opengl_dll Qt5OpenGL.dll)
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "PATH=${QT_BIN_DIR};$ENV{PATH}"
        "${WINDEPLOYQT_EXECUTABLE}" ${deploy_configuration}
        --compiler-runtime --no-translations --verbose 0 "${APP_EXECUTABLE}"
    RESULT_VARIABLE deploy_result)
if(NOT deploy_result EQUAL 0)
    message(FATAL_ERROR "Qt runtime deployment failed for ${APP_EXECUTABLE}")
endif()

get_filename_component(app_directory "${APP_EXECUTABLE}" DIRECTORY)
# VTK can load Qt OpenGL indirectly, so windeployqt may miss this DLL.
if(EXISTS "${QT_BIN_DIR}/${qt_opengl_dll}")
    file(COPY "${QT_BIN_DIR}/${qt_opengl_dll}" DESTINATION "${app_directory}")
endif()
if(RUNTIME_DIRECTORIES_FILE)
    include("${RUNTIME_DIRECTORIES_FILE}")
endif()
if(EXTRA_RUNTIME_DIRECTORIES)
    set(runtime_directories ${EXTRA_RUNTIME_DIRECTORIES})
    list(APPEND runtime_directories "${QT_BIN_DIR}")
    list(FILTER runtime_directories INCLUDE REGEX ".+")
    list(REMOVE_DUPLICATES runtime_directories)
    if(EXISTS "${RUNTIME_DEPENDENCY_TOOL}")
        set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
        set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "dumpbin")
        set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${RUNTIME_DEPENDENCY_TOOL}")
    endif()
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${APP_EXECUTABLE}"
        DIRECTORIES ${runtime_directories}
        RESOLVED_DEPENDENCIES_VAR runtime_libraries
        UNRESOLVED_DEPENDENCIES_VAR missing_libraries
        CONFLICTING_DEPENDENCIES_PREFIX runtime_conflicts
        PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
        POST_EXCLUDE_REGEXES
            ".*[/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
            ".*[/\\\\][Ss][Yy][Ss][Ww][Oo][Ww]64[/\\\\].*")
    if(missing_libraries)
        message(FATAL_ERROR "Missing runtime libraries: ${missing_libraries}. "
            "Checked dependency directories: ${runtime_directories}. "
            "Set AEROSIM_VTK_BIN and AEROSIM_CGNS_ROOT to the matching runtime locations.")
    endif()
    set(runtime_search_directories "${QT_BIN_DIR}" ${runtime_directories} "${app_directory}")
    foreach(name IN LISTS runtime_conflicts_FILENAMES)
        set(conflict_resolved FALSE)
        foreach(directory IN LISTS runtime_search_directories)
            if(EXISTS "${directory}/${name}")
                list(APPEND runtime_libraries "${directory}/${name}")
                set(conflict_resolved TRUE)
                break()
            endif()
        endforeach()
        if(NOT conflict_resolved)
            message(FATAL_ERROR "Conflicting runtime library ${name}: ${runtime_conflicts_${name}}")
        endif()
    endforeach()
    file(TO_CMAKE_PATH "$ENV{SystemRoot}" windows_directory)
    string(TOLOWER "${windows_directory}/" windows_prefix)
    foreach(library IN LISTS runtime_libraries)
        # Refresh existing copies from the currently selected SDKs.
        get_filename_component(name "${library}" NAME)
        foreach(directory IN LISTS runtime_search_directories)
            if(EXISTS "${directory}/${name}")
                set(library "${directory}/${name}")
                break()
            endif()
        endforeach()
        file(TO_CMAKE_PATH "${library}" normalized_library)
        string(TOLOWER "${normalized_library}" lower_library)
        string(FIND "${lower_library}" "${windows_prefix}" system_library)
        get_filename_component(library_directory "${library}" DIRECTORY)
        if(NOT system_library EQUAL 0 AND NOT library_directory STREQUAL app_directory)
            file(COPY "${library}" DESTINATION "${app_directory}")
        endif()
    endforeach()
endif()
file(WRITE "${app_directory}/qt.conf"
    "[Paths]\nPrefix=.\nPlugins=.\nTranslations=translations\n")
set(chinese_translation "${QT_BIN_DIR}/../translations/qtbase_zh_CN.qm")
if(EXISTS "${chinese_translation}")
    file(COPY "${chinese_translation}" DESTINATION "${app_directory}/translations")
endif()
