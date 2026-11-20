[CmdletBinding(DefaultParameterSetName = 'Gui')]
param(
    [Parameter(Position = 0)]
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string] $Configuration = 'Debug',
    [Parameter(ParameterSetName = 'Postprocess', Mandatory)]
    [switch] $Postprocess,
    [Parameter(ParameterSetName = 'Workbench', Mandatory)]
    [switch] $Workbench
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Get-ProjectPath([string] $Path) {
    if ($Path -and $Path -notlike '*-NOTFOUND') {
        if (![IO.Path]::IsPathRooted($Path)) {
            $Path = Join-Path $root $Path
        }
        [IO.Path]::GetFullPath($Path)
    }
}

function Get-DependencyPath([string] $Variable, [string] $CacheKey = $Variable) {
    $value = [Environment]::GetEnvironmentVariable($Variable)
    if (!$value) { $value = $cache[$CacheKey] }
    Get-ProjectPath $value
}

try {
    $buildDir = $env:AEROSIM_WINDOWS_BUILD_DIR
    if (!$buildDir) { $buildDir = $env:AEROSIM_BUILD_DIR }
    if (!$buildDir) { $buildDir = 'build-windows' }
    $buildDir = Get-ProjectPath $buildDir
    $cacheFile = Join-Path $buildDir 'CMakeCache.txt'
    $cache = @{}
    if (Test-Path -LiteralPath $cacheFile -PathType Leaf) {
        Get-Content -LiteralPath $cacheFile -Encoding UTF8 | ForEach-Object {
            if ($_ -match '^([^#/:][^:]*):[^=]+=(.*)$') {
                $cache[$Matches[1]] = $Matches[2]
            }
        }
    }

    $outputDir = $buildDir
    $exeName = 'aerosim_gui.exe'
    if ($Postprocess) {
        $outputDir = Join-Path $buildDir 'postprocess'
        $exeName = 'aerosim_postprocess.exe'
    }
    $exe = Join-Path $outputDir "$Configuration/$exeName"
    if (!(Test-Path -LiteralPath $exe -PathType Leaf) -and
        (!$cache['CMAKE_BUILD_TYPE'] -or $cache['CMAKE_BUILD_TYPE'] -eq $Configuration)) {
        $exe = Join-Path $outputDir $exeName
    }
    if (!(Test-Path -LiteralPath $exe -PathType Leaf)) {
        throw "Cannot find $exeName ($Configuration) in '$outputDir'. Build it first or check AEROSIM_BUILD_DIR."
    }

    $qtRoot = Get-DependencyPath 'AEROSIM_QT_ROOT'
    if (!$qtRoot) {
        $qtPackage = Get-ProjectPath $cache['Qt5_DIR']
        if ($qtPackage) { $qtRoot = [IO.Path]::GetFullPath("$qtPackage/../../..") }
    }
    if (!$qtRoot) { throw 'Set AEROSIM_QT_ROOT to the Qt SDK used to build this program.' }
    $qtBin = Join-Path $qtRoot 'bin'
    $qtPlugins = Join-Path $qtRoot 'plugins'
    $qtPlatforms = Join-Path $qtPlugins 'platforms'
    $qtSuffix = if ($Configuration -eq 'Debug') { 'd' } else { '' }
    if (!(Test-Path -LiteralPath "$qtBin/Qt5Core$qtSuffix.dll" -PathType Leaf) -or
        !(Test-Path -LiteralPath "$qtPlatforms/qwindows$qtSuffix.dll" -PathType Leaf)) {
        throw "Missing Qt $Configuration runtime or Windows platform plugin under '$qtRoot'. Check AEROSIM_QT_ROOT."
    }

    $vtkBin = Get-DependencyPath 'AEROSIM_VTK_BIN'
    $vtkDir = Get-DependencyPath 'AEROSIM_VTK_DIR' 'VTK_DIR'
    if (!$vtkBin -and $vtkDir) {
        # VTKConfig.cmake is at the build root or under lib/cmake/vtk-8.2.
        $vtkBin = @($vtkDir, [IO.Path]::GetFullPath("$vtkDir/../../..")) |
            ForEach-Object { "$_/bin/$Configuration"; "$_/bin" } |
            Where-Object { Test-Path -LiteralPath "$_/vtkCommonCore-8.2.dll" -PathType Leaf } |
            Select-Object -First 1
    }

    $runtimeDirs = @($qtBin, $vtkBin, (Get-DependencyPath 'AEROSIM_CGNS_BIN'))
    $cgnsRoot = Get-DependencyPath 'AEROSIM_CGNS_ROOT'
    if ($cgnsRoot) {
        $runtimeDirs += "$cgnsRoot/bin/$Configuration", "$cgnsRoot/bin", "$cgnsRoot/src/$Configuration"
    }
    $cgnsLib = Get-ProjectPath $cache['AEROSIM_CGNS_LIBRARY']
    if ($cgnsLib) {
        $cgnsLibDir = Split-Path -Parent $cgnsLib
        $cgnsPrefix = Split-Path -Parent $cgnsLibDir
        $runtimeDirs += $cgnsLibDir, "$cgnsPrefix/bin/$Configuration", "$cgnsPrefix/bin"
    }
    $runtimeDirs += Get-DependencyPath 'AEROSIM_HDF5_BIN'
    $qwtRoot = Get-DependencyPath 'AEROSIM_QWT_ROOT'
    if (!$qwtRoot) {
        $qwtPackage = Get-ProjectPath $cache['Qwt_DIR']
        if ($qwtPackage) { $qwtRoot = [IO.Path]::GetFullPath("$qwtPackage/../../..") }
    }
    if ($qwtRoot) { $runtimeDirs += "$qwtRoot/bin/$Configuration", "$qwtRoot/bin" }
    $runtimeDirs = $runtimeDirs | Where-Object {
        $_ -and (Test-Path -LiteralPath $_ -PathType Container)
    } | Select-Object -Unique

    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $exe
    $info.WorkingDirectory = Split-Path -Parent $exe
    $info.UseShellExecute = $false
    if ($Workbench) { $info.Arguments = '--workbench' }
    $info.EnvironmentVariables['PATH'] = (@($runtimeDirs) + $env:PATH) -join ';'
    $info.EnvironmentVariables['QT_PLUGIN_PATH'] = $qtPlugins
    $info.EnvironmentVariables['QT_QPA_PLATFORM_PLUGIN_PATH'] = $qtPlatforms
    $info.EnvironmentVariables['QT_QPA_PLATFORM'] = 'windows'
    $app = [System.Diagnostics.Process]::Start($info)
    Write-Host "[INFO] Started $exeName (PID $($app.Id))."
} catch {
    Write-Host "[ERROR] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
