@echo off
setlocal

if /i "%~1"=="full" (
    set "BUILD_CURVES=ON"
    set "BUILD_TARGETS=aerosim_gui aerosim_monitor_charts"
) else if /i "%~1"=="no-qwt" (
    set "BUILD_CURVES=OFF"
    set "BUILD_TARGETS=aerosim_gui"
) else (
    echo Usage: %~nx0 ^<full^|no-qwt^> [run]
    exit /b 2
)
if not "%~2"=="" if /i not "%~2"=="run" (
    echo Usage: %~nx0 ^<full^|no-qwt^> [run]
    exit /b 2
)

for %%I in ("%~dp0..") do set "SOURCE_DIR=%%~fI"
if not defined AEROSIM_WINDOWS_BUILD_DIR set "AEROSIM_WINDOWS_BUILD_DIR=%SOURCE_DIR%\build-windows"
if not defined AEROSIM_WINDOWS_SEED_CACHE set "AEROSIM_WINDOWS_SEED_CACHE=E:\Temp\AeroSimWorkbench-intelligent-data-build\CMakeCache.txt"
if not defined AEROSIM_WINDOWS_TOOLS_DIR set "AEROSIM_WINDOWS_TOOLS_DIR=E:\Desktop\AeroSimWorkbench\.tools"
if not defined AEROSIM_WINDOWS_VS_DIR set "AEROSIM_WINDOWS_VS_DIR=C:\Program Files\Microsoft Visual Studio\18\Community"

set "CMAKE_EXE=%AEROSIM_WINDOWS_TOOLS_DIR%\cmake-3.18.2-win64-x64\bin\cmake.exe"
set "QT_DIR=%AEROSIM_WINDOWS_TOOLS_DIR%\Qt\5.12.12\msvc2017_64"
set "VCVARS=%AEROSIM_WINDOWS_VS_DIR%\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%CMAKE_EXE%" (
    echo CMake was not found at "%CMAKE_EXE%".
    exit /b 1
)
if not exist "%VCVARS%" (
    echo Visual Studio environment script was not found at "%VCVARS%".
    exit /b 1
)

set "PATH=%QT_DIR%\bin;%AEROSIM_WINDOWS_VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
set "QT_QPA_PLATFORM_PLUGIN_PATH=%QT_DIR%\plugins\platforms"
call "%VCVARS%" -vcvars_ver=14.44
if errorlevel 1 exit /b 1

set "BUILD_OPTIONS=-DAEROSIM_BUILD_GUI=ON -DAEROSIM_BUILD_VISUALIZATION=ON -DAEROSIM_BUILD_CURVES=%BUILD_CURVES%"
if exist "%AEROSIM_WINDOWS_BUILD_DIR%\CMakeCache.txt" (
    "%CMAKE_EXE%" -S "%SOURCE_DIR%" -B "%AEROSIM_WINDOWS_BUILD_DIR%" -C "%SOURCE_DIR%\cmake\SeedWindowsBuild.cmake" %BUILD_OPTIONS%
) else (
    "%CMAKE_EXE%" -S "%SOURCE_DIR%" -B "%AEROSIM_WINDOWS_BUILD_DIR%" -G Ninja -C "%SOURCE_DIR%\cmake\SeedWindowsBuild.cmake" "-DQt5_DIR=%QT_DIR%\lib\cmake\Qt5" %BUILD_OPTIONS%
)
if errorlevel 1 exit /b 1

"%CMAKE_EXE%" --build "%AEROSIM_WINDOWS_BUILD_DIR%" --target %BUILD_TARGETS%
if errorlevel 1 (
    if /i "%~1"=="full" echo Full mode requires Qwt and the aerosim_monitor_charts target.
    exit /b 1
)

if /i "%~2"=="run" (
    "%AEROSIM_WINDOWS_BUILD_DIR%\aerosim_gui.exe" --workbench
    if errorlevel 1 exit /b 1
)
exit /b 0
