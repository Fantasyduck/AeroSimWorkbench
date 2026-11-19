$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$transcriptStarted = $false

function Read-Cache([string] $Path) {
    $entries = @{}
    if (Test-Path -LiteralPath $Path) {
        foreach ($line in Get-Content -LiteralPath $Path) {
            if ($line -match '^([^#/:][^:]*):[^=]+=(.*)$') {
                $entries[$Matches[1]] = $Matches[2]
            }
        }
    }
    return $entries
}

function Run-CMake([string[]] $Arguments) {
    & $script:cmake @Arguments
    if ($LASTEXITCODE -ne 0) { throw "CMake failed with exit code $LASTEXITCODE." }
}

function Download-Archive([string] $Url, [string] $Destination) {
    $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
    if ($curl) {
        Write-Host '[INFO] Downloading with curl.exe...'
        $info = New-Object System.Diagnostics.ProcessStartInfo
        $info.FileName = $curl.Source
        $info.Arguments = "--fail --location --silent --show-error --connect-timeout 15 --max-time 180 --output `"$Destination`" `"$Url`""
        $info.UseShellExecute = $false
        $info.CreateNoWindow = $true
        $info.RedirectStandardError = $true
        $process = New-Object System.Diagnostics.Process
        $process.StartInfo = $info
        try {
            if (-not $process.Start()) { throw 'Could not start curl.exe.' }
            $errorText = $process.StandardError.ReadToEnd()
            $process.WaitForExit()
            if ($process.ExitCode -eq 0) { return }
            Write-Warning "curl.exe failed ($($process.ExitCode)): $errorText"
        } catch {
            Write-Warning "curl.exe could not complete the download: $($_.Exception.Message)"
        } finally {
            $process.Dispose()
        }
    }
    Write-Host '[INFO] Downloading with Windows PowerShell...'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Destination -TimeoutSec 180
}

try {
    Import-Module Microsoft.PowerShell.Utility, Microsoft.PowerShell.Archive
    $projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $logDirectory = Join-Path $projectRoot 'build-qwt'
    New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
    Start-Transcript -Path (Join-Path $logDirectory 'setup-last.log') -Force | Out-Null
    $transcriptStarted = $true
    Write-Host "[INFO] Log: $logDirectory\setup-last.log"
    $mainBuild = $env:AEROSIM_WINDOWS_BUILD_DIR
    if (-not $mainBuild) { $mainBuild = $env:AEROSIM_BUILD_DIR }
    if (-not $mainBuild) { $mainBuild = Join-Path $projectRoot 'build-windows' }
    $cache = Read-Cache (Join-Path $mainBuild 'CMakeCache.txt')
    $cmake = $env:AEROSIM_CMAKE_EXE
    if (-not $cmake) { $cmake = $cache['CMAKE_COMMAND'] }
    if (-not $cmake) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
    $qtRoot = $env:AEROSIM_QT_ROOT
    if (-not $qtRoot -and $cache['Qt5_DIR']) {
        $qtRoot = [IO.Path]::GetFullPath((Join-Path $cache['Qt5_DIR'] '../../..'))
    }
    if (-not $qtRoot -and $env:AEROSIM_WINDOWS_TOOLS_DIR) {
        $qtRoot = Join-Path $env:AEROSIM_WINDOWS_TOOLS_DIR 'Qt/5.12.12/msvc2017_64'
    }
    if (-not $qtRoot -and $env:QTDIR) { $qtRoot = $env:QTDIR }
    if (-not $qtRoot -or -not (Test-Path -LiteralPath "$qtRoot/bin/qmake.exe")) {
        throw 'Set AEROSIM_QT_ROOT to the Qt 5.12.12 msvc2017_64 installation.'
    }
    $qtVersion = & "$qtRoot/bin/qmake.exe" -query QT_VERSION
    if ($LASTEXITCODE -ne 0 -or $qtVersion.Trim() -ne '5.12.12') {
        throw 'This project requires Qt 5.12.12.'
    }

    # Reuse the project's selected compiler. Fresh setups select v141 x64.
    $vsRoot = $env:AEROSIM_WINDOWS_VS_DIR
    $toolset = $env:AEROSIM_WINDOWS_VCVARS_VER
    if (-not $vsRoot -and -not $env:VSCMD_VER) {
        $compiler = $cache['CMAKE_CXX_COMPILER'] -replace '\\', '/'
        if ($compiler -match '^(.*)/VC/Tools/MSVC/([^/]+)/bin/Host[^/]+/[^/]+/cl.exe$') {
            $vsRoot = $Matches[1]
            $toolset = $Matches[2]
        } else {
            $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
            if (Test-Path -LiteralPath $vswhere) {
                $vsRoot = & $vswhere -latest -products '*' -version '[15.0,16.0)' -property installationPath
                if (-not $vsRoot) {
                    $vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.v141.x86.x64 -property installationPath
                }
            }
        }
    }
    if ($vsRoot) {
        if (-not $toolset) { $toolset = '14.1' }
        $vcvars = Join-Path $vsRoot 'VC/Auxiliary/Build/vcvars64.bat'
        if (-not (Test-Path -LiteralPath $vcvars)) { throw "Missing compiler setup: $vcvars" }
        $environment = & $env:ComSpec /d /s /c "call `"$vcvars`" -vcvars_ver=$toolset >nul && set"
        if ($LASTEXITCODE -ne 0) { throw "Could not initialize MSVC $toolset x64." }
        foreach ($line in $environment) {
            if ($line -match '^([^=]+)=(.*)$') {
                [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
            }
        }
    }
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw 'Use a VS2017 x64 developer prompt, or configure AEROSIM_WINDOWS_VS_DIR.'
    }

    $revision = '29d0b994b54f92777b8d60e0ebe6c3d62d70db74'
    $sha256 = 'FD008054C807C8119952B6F8C165ED7088A6FD48A5630C1C150F7550397D06B7'
    $qwtRoot = Join-Path $projectRoot 'third_party/QWT'
    $buildRoot = Join-Path $projectRoot 'build-qwt'
    if (-not (Test-Path -LiteralPath "$qwtRoot/CMakeLists.txt")) {
        if (Test-Path -LiteralPath $qwtRoot) {
            throw "$qwtRoot already exists without Qwt sources. Move it aside before retrying."
        }
        $downloads = Join-Path $buildRoot 'downloads'
        New-Item -ItemType Directory -Force -Path $downloads | Out-Null
        $archive = Join-Path $downloads "qwt-$revision.zip"
        if (-not (Test-Path -LiteralPath $archive)) {
            $downloadUrl = "https://codeload.github.com/czyt1988/QWT/zip/$revision"
            Write-Host "[INFO] Downloading Qwt 7.5.2 from $downloadUrl"
            for ($attempt = 1; $attempt -le 3; $attempt++) {
                try {
                    Write-Host "[INFO] Download attempt $attempt/3..."
                    Download-Archive $downloadUrl "$archive.part"
                    break
                } catch {
                    Write-Warning "Download failed: $($_.Exception.Message)"
                    if ($attempt -eq 3) {
                        throw "Download failed after 3 attempts. For offline setup, copy the verified archive from another computer to $archive and rerun this script."
                    }
                    Start-Sleep -Seconds 2
                }
            }
            Write-Host '[INFO] Download finished. Checking SHA256...'
            if ((Get-FileHash -LiteralPath "$archive.part" -Algorithm SHA256).Hash -ne $sha256) {
                throw "Downloaded archive checksum mismatch: $archive.part"
            }
            Move-Item -LiteralPath "$archive.part" -Destination $archive
        }
        if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $sha256) {
            throw "Cached archive checksum mismatch: $archive"
        }
        $stage = Join-Path $downloads ([Guid]::NewGuid().ToString())
        Write-Host '[INFO] Extracting Qwt sources...'
        Expand-Archive -LiteralPath $archive -DestinationPath $stage
        $sourcePath = [IO.Path]::GetFullPath((Join-Path $stage "QWT-$revision"))
        if (-not $sourcePath.StartsWith($buildRoot + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase) -or
            -not $qwtRoot.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Qwt extraction paths must stay inside this checkout.'
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $qwtRoot -Parent) | Out-Null
        Move-Item -LiteralPath $sourcePath -Destination $qwtRoot
    }

    $sourceConfig = Get-Content -LiteralPath "$qwtRoot/CMakeLists.txt" -Raw
    if ($sourceConfig -notmatch 'set\(QWT_VERSION_MAJOR 7\)' -or
        $sourceConfig -notmatch 'set\(QWT_VERSION_MINOR 5\)' -or
        $sourceConfig -notmatch 'set\(QWT_VERSION_PATCH 2\)') {
        throw 'third_party/QWT must contain Qwt 7.5.2 sources.'
    }
    $arguments = @('-S', (Join-Path $projectRoot 'cmake/qwt'), '-B', (Join-Path $buildRoot 'build'),
        '-DCMAKE_BUILD_TYPE=Debug', "-DQt5_DIR=$qtRoot/lib/cmake/Qt5", "-DAEROSIM_QWT_SOURCE_DIR=$qwtRoot")
    if (-not (Test-Path -LiteralPath "$buildRoot/build/CMakeCache.txt")) {
        $generator = $cache['CMAKE_GENERATOR']
        if (-not $generator) { $generator = $env:AEROSIM_WINDOWS_GENERATOR }
        if (-not $generator) { $generator = 'Visual Studio 15 2017 Win64' }
        $arguments += @('-G', $generator)
        if ($generator -match '^Ninja' -and $cache['CMAKE_MAKE_PROGRAM']) {
            $arguments += "-DCMAKE_MAKE_PROGRAM=$($cache['CMAKE_MAKE_PROGRAM'])"
        }
        if ($generator -match '^Visual Studio' -and $generator -notmatch 'Win64') {
            $arguments += @('-A', 'x64')
            if ($cache['CMAKE_GENERATOR_TOOLSET']) {
                $arguments += @('-T', $cache['CMAKE_GENERATOR_TOOLSET'])
            } elseif ($toolset -eq '14.1') {
                $arguments += @('-T', 'v141')
            }
        }
    }
    Write-Host "Building Qwt 7.5.2 x64 Debug with Qt $qtVersion..."
    Run-CMake $arguments
    Run-CMake @('--build', "$buildRoot/build", '--config', 'Debug', '--target', 'qwt7', '--parallel', '4')
    if (-not (Test-Path -LiteralPath "$qwtRoot/lib/qwt7.lib")) { throw 'qwt7.lib was not generated.' }
    Write-Host "[OK] Qwt is ready: $qwtRoot"
    exit 0
} catch {
    Write-Error -Message $_.Exception.Message -ErrorAction Continue
    exit 1
} finally {
    if ($transcriptStarted) { Stop-Transcript | Out-Null }
}
