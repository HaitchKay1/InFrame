#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (Get-Process obs64 -ErrorAction SilentlyContinue) { throw 'Close OBS Studio before installing SteadyFrame.' }
$Source = Join-Path $PSScriptRoot 'steadyframe'
if (!(Test-Path "$Source/bin/64bit/steadyframe.dll")) { throw 'Plugin DLL is missing. Use the built Windows package, not the source ZIP.' }
if (!(Test-Path "$Source/data/models/face_detection_yunet_2023mar.onnx")) { throw 'Face model is missing from this package.' }
$Target = Join-Path $env:ProgramData 'obs-studio/plugins/steadyframe'
if (Test-Path $Target) {
    $BackupRoot = Join-Path $env:ProgramData 'SteadyFrame/backups'
    New-Item -ItemType Directory -Force $BackupRoot | Out-Null
    $Backup = Join-Path $BackupRoot ([DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff'))
    Copy-Item -LiteralPath $Target -Destination $Backup -Recurse
    Write-Host "Previous version backed up to $Backup"
}
New-Item -ItemType Directory -Force $Target | Out-Null
Copy-Item "$Source/*" $Target -Recurse -Force
Write-Host 'Installed. Start OBS > camera source > Filters > Effect Filters > + > SteadyFrame - Face Tracking.'
Write-Host "To uninstall later: close OBS and remove only $Target"
Read-Host 'Press Enter to close'
