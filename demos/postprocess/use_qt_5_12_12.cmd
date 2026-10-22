@echo off
setlocal
set "SDK=C:\Qt\5.12.12\msvc2017_64"
set "VENV=%~dp0..\..\.venv-qt5"
set "PYSIDE=%VENV%\Lib\site-packages\PySide2"
set "BACKUP=%VENV%\pyside2-qt-5.12.6-backup"

if not exist "%SDK%\bin\Qt5Core.dll" (
    echo Qt 5.12.12 SDK not found: %SDK%
    exit /b 1
)
if not exist "%PYSIDE%\Qt5Core.dll" (
    echo Install demos\postprocess\requirements.txt into .venv-qt5 first.
    exit /b 1
)

if not exist "%BACKUP%" mkdir "%BACKUP%"
if not exist "%BACKUP%\platforms" mkdir "%BACKUP%\platforms"
for %%M in (Core Gui Widgets OpenGL) do (
    if not exist "%BACKUP%\Qt5%%M.dll" copy /Y "%PYSIDE%\Qt5%%M.dll" "%BACKUP%\Qt5%%M.dll" >nul
    copy /Y "%SDK%\bin\Qt5%%M.dll" "%PYSIDE%\Qt5%%M.dll" >nul || exit /b 1
)
if not exist "%BACKUP%\platforms\qwindows.dll" copy /Y "%PYSIDE%\plugins\platforms\qwindows.dll" "%BACKUP%\platforms\qwindows.dll" >nul
copy /Y "%SDK%\plugins\platforms\qwindows.dll" "%PYSIDE%\plugins\platforms\qwindows.dll" >nul || exit /b 1

"%VENV%\Scripts\python.exe" -c "from PySide2.QtCore import qVersion; print('Post-processing Qt runtime:', qVersion()); assert qVersion() == '5.12.12'"
exit /b %errorlevel%
