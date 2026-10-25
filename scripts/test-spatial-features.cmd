@echo off
setlocal
set "ROOT=%~dp0.."
if not defined AEROSIM_QT_ROOT (
    echo Set AEROSIM_QT_ROOT to Qt 5.12.12 msvc2017_64.
    exit /b 2
)
if not defined AEROSIM_VTK_DIR (
    echo Set AEROSIM_VTK_DIR to VTK 8.2.0 lib/cmake/vtk-8.2.
    exit /b 2
)
if not defined AEROSIM_VTK_BIN (
    echo Set AEROSIM_VTK_BIN to the VTK 8.2.0 Debug DLL directory.
    exit /b 2
)
if not defined AEROSIM_FEATURES_BUILD_DIR set "AEROSIM_FEATURES_BUILD_DIR=%ROOT%\build-spatial-features"
set "PATH=%AEROSIM_QT_ROOT%\bin;%AEROSIM_VTK_BIN%;%PATH%"
cmake -S "%ROOT%\modules\visualization" -B "%AEROSIM_FEATURES_BUILD_DIR%" -G "Visual Studio 15 2017 Win64" ^
    "-DCMAKE_PREFIX_PATH=%AEROSIM_QT_ROOT%" "-DVTK_DIR=%AEROSIM_VTK_DIR%" -DAEROSIM_BUILD_TESTS=ON
if errorlevel 1 exit /b %errorlevel%
cmake --build "%AEROSIM_FEATURES_BUILD_DIR%" --config Debug --target aerosim_spatial_features_test --parallel 6
if errorlevel 1 exit /b %errorlevel%
"%AEROSIM_FEATURES_BUILD_DIR%\Debug\aerosim_spatial_features_test.exe" "%ROOT%\tests\fixtures\postprocess"
exit /b %errorlevel%
