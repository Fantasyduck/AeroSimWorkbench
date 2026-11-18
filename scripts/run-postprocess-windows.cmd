@echo off
setlocal
cd /d "%~dp0.."

rem Configure dependency locations here, through environment variables, or in
rem aerosim-windows.local.cmd at the checkout root. Existing CMake settings
rem are reused when no override is supplied.
rem Example: set "AEROSIM_QWT_ROOT=D:\Dependencies\QWT"
rem Example: set "AEROSIM_QT_ROOT=D:\Qt\5.12.12\msvc2017_64"
rem Example: set "AEROSIM_SARIBBON_SOURCE_DIR=D:\Dependencies\SARibbon"
rem Example: set "AEROSIM_EIGEN_ROOT=D:\Dependencies\eigen"
if exist "%CD%\aerosim-windows.local.cmd" call "%CD%\aerosim-windows.local.cmd"

if not defined AEROSIM_WINDOWS_BUILD_DIR if defined AEROSIM_BUILD_DIR set "AEROSIM_WINDOWS_BUILD_DIR=%AEROSIM_BUILD_DIR%"
if not defined AEROSIM_WINDOWS_BUILD_DIR set "AEROSIM_WINDOWS_BUILD_DIR=%CD%\build-windows"
for %%I in ("%AEROSIM_WINDOWS_BUILD_DIR%") do set "AEROSIM_WINDOWS_BUILD_DIR=%%~fI"

rem Prefer the managed dependency, then reuse an existing external Qwt setup.
if not defined AEROSIM_QWT_ROOT if exist "%CD%\third_party\QWT\lib\qwt7.lib" set "AEROSIM_QWT_ROOT=%CD%\third_party\QWT"
if not defined AEROSIM_QWT_ROOT if exist "%AEROSIM_WINDOWS_BUILD_DIR%\CMakeCache.txt" (
    for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"AEROSIM_QWT_ROOT:PATH=" "%AEROSIM_WINDOWS_BUILD_DIR%\CMakeCache.txt"') do set "AEROSIM_QWT_ROOT=%%B"
)
if defined AEROSIM_QWT_ROOT echo [INFO] Qwt: "%AEROSIM_QWT_ROOT%"
if not defined AEROSIM_QWT_ROOT call "%~dp0setup-qwt-windows.cmd" --no-pause
if errorlevel 1 exit /b %errorlevel%
if not exist "%AEROSIM_QWT_ROOT%" (
    echo [ERROR] Qwt directory does not exist: "%AEROSIM_QWT_ROOT%"
    exit /b 2
)

set "POSTPROCESS_DIR=%AEROSIM_WINDOWS_BUILD_DIR%\postprocess"
set "POSTPROCESS_EXE=%POSTPROCESS_DIR%\aerosim_postprocess.exe"
if exist "%POSTPROCESS_DIR%\Debug\aerosim_postprocess.exe" set "POSTPROCESS_EXE=%POSTPROCESS_DIR%\Debug\aerosim_postprocess.exe"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0stop-postprocess-instance.ps1" "%POSTPROCESS_EXE%"
if errorlevel 1 exit /b %errorlevel%

call "%~dp0build-workbench-windows.cmd" postprocess
if errorlevel 1 exit /b %errorlevel%

rem Locate either the Visual Studio or single-configuration executable.
if exist "%POSTPROCESS_DIR%\Debug\aerosim_postprocess.exe" set "POSTPROCESS_EXE=%POSTPROCESS_DIR%\Debug\aerosim_postprocess.exe"
if not exist "%POSTPROCESS_EXE%" (
    echo [ERROR] Postprocess executable was not found: "%POSTPROCESS_EXE%"
    exit /b 2
)
rem Qt runtime deployment is performed by the executable's CMake build target.
if defined AEROSIM_VTK_BIN set "PATH=%AEROSIM_VTK_BIN%;%PATH%"
if defined AEROSIM_CGNS_ROOT set "PATH=%AEROSIM_CGNS_ROOT%\bin;%PATH%"

rem Session 0 has no interactive desktop.
powershell.exe -NoProfile -Command "if ((Get-Process -Id $PID).SessionId -eq 0) { exit 1 }"
if errorlevel 1 (
    echo [INFO] Build succeeded. Run this script on the Windows desktop to open the window.
    exit /b 0
)

rem Use the deployed runtime next to the executable when launching the GUI.
set "AEROSIM_POSTPROCESS_EXECUTABLE=%POSTPROCESS_EXE%"
powershell.exe -NoProfile -WindowStyle Hidden -Command "$info=New-Object System.Diagnostics.ProcessStartInfo; $info.FileName=$env:AEROSIM_POSTPROCESS_EXECUTABLE; $info.WorkingDirectory=[System.IO.Path]::GetDirectoryName($info.FileName); $info.UseShellExecute=$false; [System.Diagnostics.Process]::Start($info) | Out-Null"
exit /b %errorlevel%
