if(NOT EXISTS "${APP_EXECUTABLE}" OR NOT EXISTS "${WINDEPLOYQT_EXECUTABLE}")
    message(FATAL_ERROR "The application and the Qt deployment tool must exist")
endif()

if(BUILD_CONFIGURATION STREQUAL "Debug")
    set(deploy_configuration --debug)
else()
    set(deploy_configuration --release)
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
file(WRITE "${app_directory}/qt.conf"
    "[Paths]\nPrefix=.\nPlugins=.\nTranslations=translations\n")
set(chinese_translation "${QT_BIN_DIR}/../translations/qtbase_zh_CN.qm")
if(EXISTS "${chinese_translation}")
    file(COPY "${chinese_translation}" DESTINATION "${app_directory}/translations")
endif()
