@echo off
setlocal EnableDelayedExpansion
set "ROOT=%~dp0.."
if not defined AEROSIM_QT_ROOT (
    echo Set AEROSIM_QT_ROOT to the Qt 5.12.12 msvc2017_64 directory.
    exit /b 2
)
if not defined AEROSIM_BUILD_DIR set "AEROSIM_BUILD_DIR=%ROOT%\build-windows"
if not defined AEROSIM_BUILD_GUI set "AEROSIM_BUILD_GUI=ON"
if not defined AEROSIM_BUILD_STACK_PROBE set "AEROSIM_BUILD_STACK_PROBE=OFF"
set "PATH=%AEROSIM_QT_ROOT%\bin;%PATH%"
if defined AEROSIM_VTK_BIN set "PATH=%AEROSIM_VTK_BIN%;%PATH%"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=all"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Debug"

if /I "%AEROSIM_BUILD_STACK_PROBE%"=="ON" (
    cmake -S "%ROOT%" -B "%AEROSIM_BUILD_DIR%" -G "Visual Studio 15 2017 Win64" ^
        "-DCMAKE_PREFIX_PATH=%AEROSIM_QT_ROOT%" ^
        "-DAEROSIM_BUILD_GUI=%AEROSIM_BUILD_GUI%" ^
        "-DAEROSIM_BUILD_STACK_PROBE=ON" ^
        "-DVTK_DIR=%AEROSIM_VTK_DIR%" ^
        "-DSARIBBON_SOURCE_DIR=%AEROSIM_SARIBBON_SOURCE_DIR%"
) else (
    cmake -S "%ROOT%" -B "%AEROSIM_BUILD_DIR%" -G "Visual Studio 15 2017 Win64" ^
        "-DCMAKE_PREFIX_PATH=%AEROSIM_QT_ROOT%" ^
        "-DAEROSIM_BUILD_GUI=%AEROSIM_BUILD_GUI%" ^
        "-DAEROSIM_BUILD_STACK_PROBE=OFF"
)
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
    set "TEST_RESULT=%errorlevel%"
    popd
    exit /b !TEST_RESULT!
)
exit /b 0
