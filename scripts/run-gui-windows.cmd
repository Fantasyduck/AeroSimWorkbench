@echo off
setlocal
cd /d "%~dp0.."
if exist "aerosim-windows.local.cmd" call "aerosim-windows.local.cmd"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-gui-windows.ps1" %*
exit /b %errorlevel%
