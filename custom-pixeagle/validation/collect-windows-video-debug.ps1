[CmdletBinding()]
param([string]$Executable)

$ErrorActionPreference = 'Stop'
if (-not $Executable) {
    $roots = @(
        "$env:ProgramFiles\PixEagle-QGroundControl",
        "$env:ProgramFiles\QGroundControl",
        "$env:LOCALAPPDATA\Programs\PixEagle-QGroundControl",
        "$env:LOCALAPPDATA\Programs\QGroundControl"
    )
    $found = @(Get-ChildItem $roots -Filter 'PixEagle-QGroundControl.exe' -Recurse -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName -Unique)
    if ($found.Count -ne 1) {
        throw 'Specify the installed PixEagle executable with -Executable; no unique custom installation was found.'
    }
    $Executable = $found[0]
}
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$desktop = [Environment]::GetFolderPath('Desktop')
$folder = Join-Path $desktop ('pixeagle-qgc-diagnostics-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $folder | Out-Null
$stdout = Join-Path $folder 'stdout.log'
$stderr = Join-Path $folder 'stderr.log'
$env:QGC_LOG_LEVEL = 'info'
# Keep the render-thread dirty-item dump disabled, including on older builds.
$env:QT_LOGGING_RULES = 'qt.quick.dirty.debug=false'
$env:GST_DEBUG = '2'
@("Executable: $Executable", "SHA256: $((Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash)") |
    Set-Content -LiteralPath (Join-Path $folder 'run.txt')
Write-Host "Starting: $Executable"
Write-Host "Logs: $folder"
Write-Host 'Sign in, wait 30 seconds in Fly View, then close QGC normally.'
$process = Start-Process -FilePath $Executable -ArgumentList @(
    '--logging', 'qgc.pixeagle.camera,Video.GStreamer.,VideoManager.', '--log-output'
) -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
$process.WaitForExit()
"Exit code: $($process.ExitCode)" | Add-Content -LiteralPath (Join-Path $folder 'run.txt')
Write-Host "QGC closed. Logs are saved in: $folder"
