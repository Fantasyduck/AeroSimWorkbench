@echo off
setlocal
if exist "%~dp0..\aerosim-windows.local.cmd" call "%~dp0..\aerosim-windows.local.cmd"
if not exist "%~dp0setup-qwt-windows.ps1" (
    echo [ERROR] Missing setup-qwt-windows.ps1 next to this script.
    goto failed
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-qwt-windows.ps1"
if not "%errorlevel%"=="0" goto failed
echo [OK] Qwt configuration completed.
if /i not "%~1"=="--no-pause" pause
endlocal
for %%I in ("%~dp0..\third_party\QWT") do set "AEROSIM_QWT_ROOT=%%~fI"
exit /b 0

:failed
echo [ERROR] Qwt configuration failed. See the error above.
echo [INFO] Log file: "%~dp0..\build-qwt\setup-last.log"
if /i not "%~1"=="--no-pause" pause
exit /b 1
