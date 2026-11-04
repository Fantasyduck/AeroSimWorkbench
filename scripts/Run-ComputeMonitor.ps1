param(
    [switch]$Demo,
    [switch]$NoBuild,
    [string]$QtRoot = $env:AEROSIM_QT_ROOT,
    [string]$RibbonRoot = $env:AEROSIM_SARIBBON_SOURCE_DIR
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
# Optional machine-specific configuration is ignored by Git.
$configPath = Join-Path $repo '.vscode/monitor.local.json'
$cmake = 'cmake'
if (Test-Path -LiteralPath $configPath) {
    $config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
    if (-not $QtRoot) { $QtRoot = $config.qtRoot }
    if (-not $RibbonRoot) { $RibbonRoot = $config.ribbonRoot }
    if ($config.cmake) { $cmake = $config.cmake }
}
if (-not $QtRoot -or -not (Test-Path -LiteralPath (Join-Path $QtRoot 'bin/Qt5Cored.dll'))) {
    throw 'Set AEROSIM_QT_ROOT or pass -QtRoot (Qt 5.12.12 MSVC2017 x64).'
}
$build = Join-Path $repo 'build-monitor-main'
if (-not $NoBuild) {
    if (-not $RibbonRoot) { throw 'Set AEROSIM_SARIBBON_SOURCE_DIR or pass -RibbonRoot.' }
    & $cmake -S $repo -B $build -G 'Visual Studio 15 2017 Win64' "-DCMAKE_PREFIX_PATH=$QtRoot" "-DSARIBBON_SOURCE_DIR=$RibbonRoot" -DAEROSIM_BUILD_VISUALIZATION=OFF
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    & $cmake --build $build --config Debug --target aerosim_gui --parallel 6
    if ($LASTEXITCODE -ne 0) { throw 'Build failed. Close this checkout''s running GUI before rebuilding.' }
}
$exe = Join-Path $build 'Debug/aerosim_gui.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'GUI not built yet. Run without -NoBuild.' }
$env:PATH = (Join-Path $QtRoot 'bin') + ';' + $env:PATH
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $QtRoot 'plugins/platforms'
$env:QT_QPA_PLATFORM = 'windows'
$argument = if ($Demo) { '--monitor-demo' } else { '--monitor' }
& $exe $argument
