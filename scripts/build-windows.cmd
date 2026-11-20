@echo off
setlocal EnableDelayedExpansion
set "ROOT=%~dp0.."
rem Optional machine-local dependency paths; never committed.
if exist "%ROOT%\aerosim-windows.local.cmd" call "%ROOT%\aerosim-windows.local.cmd"
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
rem AEROSIM_BUILD_VISUALIZATION is deliberately not defaulted here. The project
rem turns visualization on together with AEROSIM_BUILD_GUI, and also supplies
rem the Qwt/Eigen error messages when they are missing; pinning OFF here would
rem silently override that and leave the caller with a placeholder chart and no
rem hint about why. Set the variable to force a value; leave it unset to take
rem the project default.
if not defined AEROSIM_BUILD_STACK_PROBE set "AEROSIM_BUILD_STACK_PROBE=OFF"
if not defined AEROSIM_BUILD_MESH_VIEWER if defined AEROSIM_VTK_DIR set "AEROSIM_BUILD_MESH_VIEWER=ON"
if not defined AEROSIM_BUILD_MESH_VIEWER set "AEROSIM_BUILD_MESH_VIEWER=OFF"
set "PATH=%AEROSIM_QT_ROOT%\bin;%PATH%"
rem Deployed GUI directories may only contain qwindowsd; CTest also needs offscreen.
set "QT_QPA_PLATFORM_PLUGIN_PATH=%AEROSIM_QT_ROOT%\plugins\platforms"
if defined AEROSIM_VTK_BIN set "PATH=%AEROSIM_VTK_BIN%;%PATH%"
if defined AEROSIM_CGNS_ROOT set "PATH=%AEROSIM_CGNS_ROOT%\bin;%PATH%"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=all"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Debug"
rem The post-processing target includes its real VTK/CGNS integration by default.
if /I "%TARGET%"=="aerosim_postprocess" if not defined AEROSIM_BUILD_VISUALIZATION set "AEROSIM_BUILD_VISUALIZATION=ON"
rem Unspecified options retain their existing CMake cache values; the optional
rem -D arguments are assembled below, next to the configure call they feed.

rem Windows cannot relink an EXE while it is running. Only close the
rem postprocess instance from this build when the requested target needs it.
if /I "%TARGET%"=="all" goto stop_postprocess_for_build
if /I "%TARGET%"=="aerosim_postprocess" goto stop_postprocess_for_build
goto postprocess_build_ready
:stop_postprocess_for_build
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0stop-postprocess-instance.ps1" "%AEROSIM_BUILD_DIR%\postprocess\Debug\aerosim_postprocess.exe"
if errorlevel 1 exit /b %errorlevel%
:postprocess_build_ready

if /I "%AEROSIM_BUILD_STACK_PROBE%"=="ON" if not defined AEROSIM_VTK_DIR (
    echo Set AEROSIM_VTK_DIR to the VTK 8.2.0 CMake package directory.
    exit /b 2
)
rem Optional arguments are assembled one variable at a time: each holds its own
rem quoted value, so a path with spaces survives, and an unset variable expands
rem to nothing instead of sending an empty -D that would override a default.
set "VIS_ARGS="
if defined AEROSIM_BUILD_VISUALIZATION set VIS_ARGS=-DAEROSIM_BUILD_VISUALIZATION=%AEROSIM_BUILD_VISUALIZATION%
set "QWT_ARGS="
if defined AEROSIM_QWT_ROOT set QWT_ARGS=-DAEROSIM_QWT_ROOT="%AEROSIM_QWT_ROOT%"
set "EIGEN_ARGS="
if defined AEROSIM_EIGEN_ROOT set EIGEN_ARGS=-DAEROSIM_EIGEN_ROOT="%AEROSIM_EIGEN_ROOT%"
set "VTK_ARGS="
if defined AEROSIM_VTK_DIR set VTK_ARGS=-DVTK_DIR="%AEROSIM_VTK_DIR%"
set "CGNS_ARGS="
if defined AEROSIM_CGNS_ROOT set CGNS_ARGS=-DAEROSIM_CGNS_ROOT="%AEROSIM_CGNS_ROOT%"

cmake -S "%ROOT%" -B "%AEROSIM_BUILD_DIR%" -G "Visual Studio 15 2017 Win64" ^
    "-DCMAKE_PREFIX_PATH=%AEROSIM_QT_ROOT%" ^
    "-DAEROSIM_BUILD_GUI=%AEROSIM_BUILD_GUI%" ^
    "-DAEROSIM_BUILD_STACK_PROBE=%AEROSIM_BUILD_STACK_PROBE%" ^
    "-DAEROSIM_BUILD_MESH_VIEWER=%AEROSIM_BUILD_MESH_VIEWER%" ^
    "-DSARIBBON_SOURCE_DIR=%AEROSIM_SARIBBON_SOURCE_DIR%" ^
    %VIS_ARGS% %QWT_ARGS% %EIGEN_ARGS% %VTK_ARGS% %CGNS_ARGS%
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
