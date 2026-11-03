param(
    [Parameter(Mandatory = $true)]
    [string] $Executable
)

$ErrorActionPreference = 'Stop'
$targetPath = [System.IO.Path]::GetFullPath($Executable)

try {
    $instances = @(Get-CimInstance Win32_Process -Filter "Name = 'aerosim_postprocess.exe'" |
        Where-Object { $_.ExecutablePath -and
            [string]::Equals($_.ExecutablePath, $targetPath, [System.StringComparison]::OrdinalIgnoreCase) })
    foreach ($instance in $instances) {
        $process = Get-Process -Id $instance.ProcessId -ErrorAction SilentlyContinue
        if (-not $process) { continue }
        try {
            Write-Host "Closing previous postprocess process $($process.Id)."
            $sentClose = $process.CloseMainWindow()
            if (-not $sentClose -or -not $process.WaitForExit(4000)) {
                if (-not $process.HasExited) { $process.Kill() }
                if (-not $process.WaitForExit(5000)) {
                    throw "Process $($process.Id) did not exit."
                }
            }
        } catch {
            if (-not $process.HasExited) { throw }
        } finally {
            $process.Dispose()
        }
    }
} catch {
    Write-Error "Unable to close the previous postprocess instance: $_"
    exit 1
}
