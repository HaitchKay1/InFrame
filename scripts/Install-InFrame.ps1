#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (Get-Process obs64 -ErrorAction SilentlyContinue) { throw 'Close OBS Studio before installing InFrame.' }
$Source = Join-Path $PSScriptRoot 'inframe'
foreach ($Required in @('bin/64bit/inframe.dll','data/crop.effect','data/models/face_detection_yunet_2023mar.onnx')) {
    if (!(Test-Path (Join-Path $Source $Required) -PathType Leaf)) { throw "Package is incomplete: $Required. Extract the entire built Windows ZIP before installing." }
}
$Target = Join-Path $env:ProgramData 'obs-studio/plugins/inframe'
# Compatibility with the original plugin's installation location.
$LegacyTarget = Join-Path $env:ProgramData 'obs-studio/plugins/steadyframe'
$Transaction = Join-Path $env:ProgramData ('InFrame/backups/' + [DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N'))
$Stage = Join-Path $Transaction 'staging'
New-Item -ItemType Directory -Force $Stage | Out-Null
Copy-Item "$Source/*" $Stage -Recurse -Force
$Previous = Join-Path $Transaction 'previous-inframe'
$Legacy = Join-Path $Transaction 'previous-plugin'
$MovedPrevious = $false
$MovedLegacy = $false
try {
    if (Test-Path $Target) { Move-Item -LiteralPath $Target -Destination $Previous; $MovedPrevious = $true }
    if (Test-Path $LegacyTarget) { Move-Item -LiteralPath $LegacyTarget -Destination $Legacy; $MovedLegacy = $true }
    New-Item -ItemType Directory -Force (Split-Path $Target -Parent) | Out-Null
    Move-Item -LiteralPath $Stage -Destination $Target
} catch {
    # Roll back any directory moves if the new installation cannot be put in place.
    if ($MovedPrevious) { Move-Item -LiteralPath $Previous -Destination $Target }
    if ($MovedLegacy) { Move-Item -LiteralPath $Legacy -Destination $LegacyTarget }
    throw
}
if ($MovedPrevious -or $MovedLegacy) { Write-Host "Previous installation backed up to $Transaction" }
Write-Host 'Installed. Start OBS > camera source > Filters > Effect Filters > + > InFrame - Face Tracking.'
Write-Host "To uninstall later: close OBS and remove only $Target"
Read-Host 'Press Enter to close'
