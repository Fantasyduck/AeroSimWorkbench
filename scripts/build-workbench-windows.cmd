@echo off
setlocal

if /i "%~1"=="full" (
    set "BUILD_CURVES=ON"
    set "BUILD_TARGETS=aerosim_gui aerosim_monitor_charts"
) else if /i "%~1"=="no-qwt" (
    set "BUILD_CURVES=OFF"
    set "BUILD_TARGETS=aerosim_gui"
) else (
    goto usage
)
if not "%~2"=="" if /i not "%~2"=="run" goto usage
if not "%~3"=="" goto usage

for %%I in ("%~dp0..") do set "SOURCE_DIR=%%~fI"
if not defined AEROSIM_WINDOWS_BUILD_DIR if defined AEROSIM_BUILD_DIR set "AEROSIM_WINDOWS_BUILD_DIR=%AEROSIM_BUILD_DIR%"
if not defined AEROSIM_WINDOWS_BUILD_DIR set "AEROSIM_WINDOWS_BUILD_DIR=%SOURCE_DIR%\build-windows"
for %%I in ("%AEROSIM_WINDOWS_BUILD_DIR%") do set "AEROSIM_WINDOWS_BUILD_DIR=%%~fI"

rem Prefer an explicit CMake, then the existing build's CMake, then PATH.
set "CMAKE_EXE=%AEROSIM_CMAKE_EXE%"
if not defined CMAKE_EXE if defined AEROSIM_WINDOWS_TOOLS_DIR set "CMAKE_EXE=%AEROSIM_WINDOWS_TOOLS_DIR%\cmake-3.18.2-win64-x64\bin\cmake.exe"
if not defined CMAKE_EXE if exist "%AEROSIM_WINDOWS_BUILD_DIR%\CMakeCache.txt" (
    for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"CMAKE_COMMAND:INTERNAL=" "%AEROSIM_WINDOWS_BUILD_DIR%\CMakeCache.txt"') do set "CMAKE_EXE=%%B"
)
if not defined CMAKE_EXE set "CMAKE_EXE=cmake"
"%CMAKE_EXE%" --version >nul 2>&1
if errorlevel 1 (
    echo CMake is unavailable. Add CMake to PATH or set AEROSIM_CMAKE_EXE.
    exit /b 2
)

rem Reuse an active developer prompt. An explicitly selected VS can initialize it.
if defined AEROSIM_WINDOWS_VS_DIR (
    if not exist "%AEROSIM_WINDOWS_VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" (
        echo AEROSIM_WINDOWS_VS_DIR must point to a Visual Studio installation.
        exit /b 2
    )
    call :initialize_vs
    if errorlevel 1 exit /b 1
)
if not defined VSCMD_VER if not defined AEROSIM_WINDOWS_VS_DIR if exist "%AEROSIM_WINDOWS_BUILD_DIR%\CMakeCache.txt" (
    call :initialize_cached_vs
    if errorlevel 1 exit /b 1
)

set "QT_ARGS="
if not defined AEROSIM_QT_ROOT if defined AEROSIM_WINDOWS_TOOLS_DIR set "AEROSIM_QT_ROOT=%AEROSIM_WINDOWS_TOOLS_DIR%\Qt\5.12.12\msvc2017_64"
if defined AEROSIM_QT_ROOT (
    if not exist "%AEROSIM_QT_ROOT%\lib\cmake\Qt5\Qt5Config.cmake" (
        echo AEROSIM_QT_ROOT must point to the Qt 5.12.12 MSVC x64 SDK.
        exit /b 2
    )
    set QT_ARGS="-DQt5_DIR=%AEROSIM_QT_ROOT%\lib\cmake\Qt5"
)
set "SARIBBON_ARGS="
if defined AEROSIM_SARIBBON_SOURCE_DIR set SARIBBON_ARGS="-DSARIBBON_SOURCE_DIR=%AEROSIM_SARIBBON_SOURCE_DIR%"
set "QWT_ARGS="
if defined AEROSIM_QWT_ROOT set QWT_ARGS="-DAEROSIM_QWT_ROOT=%AEROSIM_QWT_ROOT%"
set "EIGEN_ARGS="
if defined AEROSIM_EIGEN_ROOT set EIGEN_ARGS="-DAEROSIM_EIGEN_ROOT=%AEROSIM_EIGEN_ROOT%"
set "VTK_ARGS="
if defined AEROSIM_VTK_DIR set VTK_ARGS="-DAEROSIM_VTK_DIR=%AEROSIM_VTK_DIR%"

rem A seed cache is used only when the caller explicitly requests it.
set "SEED_ARGS="
if defined AEROSIM_WINDOWS_SEED_CACHE (
    if not exist "%AEROSIM_WINDOWS_SEED_CACHE%" (
        echo AEROSIM_WINDOWS_SEED_CACHE does not exist.
        exit /b 2
    )
    set SEED_ARGS=-C "%SOURCE_DIR%\cmake\SeedWindowsBuild.cmake"
)
set "GENERATOR_ARGS="
if not exist "%AEROSIM_WINDOWS_BUILD_DIR%\CMakeCache.txt" call :select_generator

"%CMAKE_EXE%" -S "%SOURCE_DIR%" -B "%AEROSIM_WINDOWS_BUILD_DIR%" %GENERATOR_ARGS% %SEED_ARGS% ^
    -DCMAKE_BUILD_TYPE=Debug -DAEROSIM_BUILD_GUI=ON -DAEROSIM_BUILD_VISUALIZATION=ON ^
    -DAEROSIM_BUILD_MACHINE_LEARNING=ON -DAEROSIM_BUILD_CURVES=%BUILD_CURVES% %QT_ARGS% %SARIBBON_ARGS% %QWT_ARGS% %EIGEN_ARGS% %VTK_ARGS%
if errorlevel 1 exit /b 1

"%CMAKE_EXE%" --build "%AEROSIM_WINDOWS_BUILD_DIR%" --config Debug --target %BUILD_TARGETS%
if errorlevel 1 (
    if /i "%~1"=="full" echo Full mode requires Qwt and the aerosim_monitor_charts target.
    exit /b 1
)

if /i "%~2"=="run" (
    call :run_workbench
    if errorlevel 1 exit /b 1
)
exit /b 0

:initialize_vs
if not defined AEROSIM_WINDOWS_VCVARS_VER set "AEROSIM_WINDOWS_VCVARS_VER=14.1"
call "%AEROSIM_WINDOWS_VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=%AEROSIM_WINDOWS_VCVARS_VER%
exit /b %errorlevel%

:select_generator
if not defined AEROSIM_WINDOWS_GENERATOR set "AEROSIM_WINDOWS_GENERATOR=Visual Studio 15 2017 Win64"
set GENERATOR_ARGS=-G "%AEROSIM_WINDOWS_GENERATOR%"
exit /b 0

:initialize_cached_vs
rem An existing MSVC build records the exact compiler and installed toolset.
set "CACHED_COMPILER="
for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"CMAKE_CXX_COMPILER:" "%AEROSIM_WINDOWS_BUILD_DIR%\CMakeCache.txt"') do set "CACHED_COMPILER=%%B"
if not defined CACHED_COMPILER exit /b 0
for %%I in ("%CACHED_COMPILER%\..\..\..\..\..\..\..\Auxiliary\Build\vcvars64.bat") do set "CACHED_VCVARS=%%~fI"
if not exist "%CACHED_VCVARS%" exit /b 0
for %%I in ("%CACHED_COMPILER%\..\..\..\..") do set "CACHED_MSVC_DIR=%%~fI"
for %%I in ("%CACHED_MSVC_DIR%") do set "CACHED_MSVC_VERSION=%%~nxI"
call "%CACHED_VCVARS%" -vcvars_ver=%CACHED_MSVC_VERSION%
exit /b %errorlevel%

:run_workbench
set "GUI_EXE=%AEROSIM_WINDOWS_BUILD_DIR%\aerosim_gui.exe"
if exist "%AEROSIM_WINDOWS_BUILD_DIR%\Debug\aerosim_gui.exe" set "GUI_EXE=%AEROSIM_WINDOWS_BUILD_DIR%\Debug\aerosim_gui.exe"
"%GUI_EXE%" --workbench
exit /b %errorlevel%

:usage
echo Usage: %~nx0 ^<full^|no-qwt^> [run]
echo Configure dependencies with AEROSIM_QT_ROOT and AEROSIM_SARIBBON_SOURCE_DIR.
echo Existing build caches are reused. New builds default to Visual Studio 2017 x64 Debug.
exit /b 2
