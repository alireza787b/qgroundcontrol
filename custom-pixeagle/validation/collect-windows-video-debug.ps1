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
@("Executable: $Executable", "SHA256: $((Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash)") |
    Set-Content -LiteralPath (Join-Path $folder 'run.txt')

$binaryDirectory = Split-Path -Parent $Executable
$installDirectory = Split-Path -Parent $binaryDirectory
$windows = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
$system = [ordered]@{
    productName = $windows.ProductName
    edition = $windows.EditionID
    displayVersion = $windows.DisplayVersion
    build = $windows.CurrentBuildNumber
    revision = $windows.UBR
    os64Bit = [Environment]::Is64BitOperatingSystem
    process64Bit = [Environment]::Is64BitProcess
    powershellVersion = $PSVersionTable.PSVersion.ToString()
    qtEnvironment = [ordered]@{}
    mediaFoundationFiles = @()
    mediaFoundationStartup = $null
}
foreach ($name in @('QT_MEDIA_BACKEND', 'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QSG_RHI_BACKEND')) {
    $system.qtEnvironment[$name] = [Environment]::GetEnvironmentVariable($name)
}
foreach ($name in @('mf.dll', 'mfplat.dll', 'mfreadwrite.dll', 'evr.dll')) {
    $path = Join-Path $env:WINDIR "System32\$name"
    $system.mediaFoundationFiles += [ordered]@{ name = $name; present = (Test-Path -LiteralPath $path) }
}
try {
    Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class PixEagleMediaFoundationProbe {
    [DllImport("mfplat.dll", ExactSpelling = true)]
    public static extern int MFStartup(int version, int flags);
    [DllImport("mfplat.dll", ExactSpelling = true)]
    public static extern int MFShutdown();
}
'@
    $result = [PixEagleMediaFoundationProbe]::MFStartup(0x00020070, 1)
    $system.mediaFoundationStartup = ('0x{0:X8}' -f $result)
    if ($result -eq 0) { [void][PixEagleMediaFoundationProbe]::MFShutdown() }
} catch {
    $system.mediaFoundationStartup = $_.Exception.Message
}
$system | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $folder 'system.json')

$files = @(
    (Join-Path $binaryDirectory 'qt.conf'),
    (Join-Path $installDirectory 'plugins\multimedia\ffmpegmediaplugin.dll'),
    (Join-Path $installDirectory 'plugins\multimedia\windowsmediaplugin.dll')
)
$files += @(Get-ChildItem -LiteralPath $binaryDirectory -Filter '*.dll' |
    Where-Object { $_.Name -match '^(Qt6(Core|Gui|Multimedia)|avcodec-\d+|avformat-\d+|avutil-\d+|swresample-\d+|swscale-\d+|msvcp140.*|vcruntime140.*)\.dll$' } |
    Select-Object -ExpandProperty FullName)
$inventory = foreach ($path in $files) {
    $present = Test-Path -LiteralPath $path
    [ordered]@{
        path = $path
        present = $present
        sha256 = if ($present) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } else { $null }
    }
}
ConvertTo-Json -InputObject @($inventory) -Depth 4 |
    Set-Content -LiteralPath (Join-Path $folder 'installed-media-files.json')
if (Test-Path -LiteralPath (Join-Path $binaryDirectory 'qt.conf')) {
    Copy-Item -LiteralPath (Join-Path $binaryDirectory 'qt.conf') -Destination (Join-Path $folder 'qt.conf.txt')
}

Write-Host "Starting: $Executable"
Write-Host "Logs: $folder"
Write-Host 'Sign in, wait 30 seconds in Fly View, then close QGC normally.'
$savedEnvironment = @{}
foreach ($name in @('QGC_LOG_LEVEL', 'QT_DEBUG_PLUGINS', 'QT_LOGGING_RULES', 'GST_DEBUG')) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name)
}
try {
    # Global info filtering hid the transport and plugin-loader evidence in the previous capture.
    $env:QGC_LOG_LEVEL = $null
    $env:QT_DEBUG_PLUGINS = '1'
    $env:QT_LOGGING_RULES = '*.debug=false;qt.core.plugin.*.debug=true;qt.core.library.debug=true;qt.multimedia.*.debug=true;qt.quick.dirty.debug=false;qt.qml.binding.removal=false'
    $env:GST_DEBUG = '2'
    $process = Start-Process -FilePath $Executable -ArgumentList @(
        '--logging', 'qgc.pixeagle.camera,Video.GStreamer.,VideoManager.', '--log-output'
    ) -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    # Cache the native handle before exit so Windows PowerShell retains the exit code.
    $null = $process.Handle
    $process.WaitForExit()
    "Exit code: $($process.ExitCode)" | Add-Content -LiteralPath (Join-Path $folder 'run.txt')
} finally {
    foreach ($name in $savedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name])
    }
}
Write-Host "QGC closed. Logs are saved in: $folder"
Compress-Archive -Path (Join-Path $folder '*') -DestinationPath "$folder.zip"
Write-Host "Return this diagnostics archive: $folder.zip"
