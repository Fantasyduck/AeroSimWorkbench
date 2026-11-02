@echo off
setlocal

rem Build and launch the post-processing workspace from this checkout.
rem Defaults match the validated Windows Debug environment; callers may override
rem either variable before invoking this script.
cd /d "%~dp0.."

if not defined AEROSIM_BUILD_DIR set "AEROSIM_BUILD_DIR=%CD%\build-windows"

if not defined AEROSIM_QT_ROOT set "AEROSIM_QT_ROOT=C:\Qt\5.12.12\msvc2017_64"
if not defined AEROSIM_SARIBBON_SOURCE_DIR set "AEROSIM_SARIBBON_SOURCE_DIR=C:\Users\huangzb\SARibbon"

if not exist "%AEROSIM_QT_ROOT%\bin\windeployqt.exe" (
    echo [ERROR] Qt Debug deployment tool was not found: %AEROSIM_QT_ROOT%
    exit /b 2
)
if not exist "%AEROSIM_SARIBBON_SOURCE_DIR%\src\SARibbonBar\SARibbonBar.h" (
    echo [ERROR] SARibbon source was not found: %AEROSIM_SARIBBON_SOURCE_DIR%
    exit /b 2
)

call scripts\build-windows.cmd aerosim_postprocess Debug
if errorlevel 1 exit /b %errorlevel%

"%AEROSIM_QT_ROOT%\bin\windeployqt.exe" --debug --compiler-runtime "%AEROSIM_BUILD_DIR%\postprocess\Debug\aerosim_postprocess.exe"
if errorlevel 1 exit /b %errorlevel%

start "AeroSimWorkbench Postprocess" /d "%AEROSIM_BUILD_DIR%\postprocess\Debug" aerosim_postprocess.exe
