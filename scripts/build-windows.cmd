@echo off
setlocal EnableDelayedExpansion
set "ROOT=%~dp0.."
if not defined AEROSIM_QT_ROOT (
    echo Set AEROSIM_QT_ROOT to the Qt 5.12.12 msvc2017_64 directory.
    exit /b 2
)
if not defined AEROSIM_SARIBBON_SOURCE_DIR (
    echo Set AEROSIM_SARIBBON_SOURCE_DIR to the SARibbon 2.9.3 source directory.
    exit /b 2
)
if not defined AEROSIM_BUILD_DIR set "AEROSIM_BUILD_DIR=%ROOT%\build-windows"
if not defined AEROSIM_BUILD_GUI set "AEROSIM_BUILD_GUI=ON"
if not defined AEROSIM_BUILD_VISUALIZATION set "AEROSIM_BUILD_VISUALIZATION=OFF"
if not defined AEROSIM_BUILD_STACK_PROBE set "AEROSIM_BUILD_STACK_PROBE=OFF"
set "PATH=%AEROSIM_QT_ROOT%\bin;%PATH%"
rem Deployed GUI directories may only contain qwindowsd; CTest also needs offscreen.
set "QT_QPA_PLATFORM_PLUGIN_PATH=%AEROSIM_QT_ROOT%\plugins\platforms"
if defined AEROSIM_VTK_BIN set "PATH=%AEROSIM_VTK_BIN%;%PATH%"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=all"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Debug"

if /I "%AEROSIM_BUILD_STACK_PROBE%"=="ON" if not defined AEROSIM_VTK_DIR (
    echo Set AEROSIM_VTK_DIR to the VTK 8.2.0 CMake package directory.
    exit /b 2
)
cmake -S "%ROOT%" -B "%AEROSIM_BUILD_DIR%" -G "Visual Studio 15 2017 Win64" ^
    "-DCMAKE_PREFIX_PATH=%AEROSIM_QT_ROOT%" ^
    "-DAEROSIM_BUILD_GUI=%AEROSIM_BUILD_GUI%" ^
    "-DAEROSIM_BUILD_VISUALIZATION=%AEROSIM_BUILD_VISUALIZATION%" ^
    "-DAEROSIM_BUILD_STACK_PROBE=%AEROSIM_BUILD_STACK_PROBE%" ^
    "-DVTK_DIR=%AEROSIM_VTK_DIR%" ^
    "-DSARIBBON_SOURCE_DIR=%AEROSIM_SARIBBON_SOURCE_DIR%"
if errorlevel 1 exit /b %errorlevel%

if /I "%TARGET%"=="all" (
    cmake --build "%AEROSIM_BUILD_DIR%" --config "%CONFIG%" --parallel 6
) else (
    cmake --build "%AEROSIM_BUILD_DIR%" --config "%CONFIG%" --target "%TARGET%" --parallel 6
)
if errorlevel 1 exit /b %errorlevel%

if /I "%TARGET%"=="all" (
    pushd "%AEROSIM_BUILD_DIR%"
    ctest -C "%CONFIG%" --output-on-failure
    set "TEST_RESULT=!errorlevel!"
    popd
    exit /b !TEST_RESULT!
)
exit /b 0
