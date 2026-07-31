param(
  [switch]$Vorbis,
  [switch]$UseHleKernel,
  [ValidateSet('Vita3K.exe', 'Vita3K-gl44.exe')]
  [string]$EmulatorName = 'Vita3K.exe'
)

$ErrorActionPreference = 'Stop'
$probeRoot = Split-Path -Parent $PSScriptRoot
$probeRuntime = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build/vita3k/runtime')).Path
$probeName = if($Vorbis) { 'vorbis' } else { 'audio' }
$probeCodec = if($Vorbis) { 'Vorbis' } else { 'MP3' }
$probeAsset = if($Vorbis) { 'in_full.ogg' } else { 'calibrate.mp3' }
$probeOutput = (Resolve-Path -LiteralPath (Join-Path $probeRoot "build/vita-$probeName-probe")).Path
$probeApp = Join-Path $probeRuntime 'portable/fs/ux0/app/VARC61616'
$probeExecutable = Join-Path $probeRuntime $EmulatorName
if(Get-Process -Name 'Vita3K', 'Vita3K-gl44' -ErrorAction SilentlyContinue) {
  throw 'An emulator is already running; preserve that session.'
}
if(-not (Test-Path -LiteralPath (Join-Path $probeApp "package/assets/audio/$probeAsset"))) {
  throw 'The isolated VitaArc installation must contain the local game audio asset.'
}
Copy-Item -LiteralPath (Join-Path $probeOutput "$probeName-probe.self") -Destination (Join-Path $probeApp "$probeName-probe.self")

$probeFirmware = Join-Path $probeRuntime 'portable/fs/os0/kd/bootimage.skprx'
$probeBackup = $probeFirmware + '.vitaarc-probe-disabled'
$probeFirmwareHash = $null
$probeMovedFirmware = $false
try {
  if($UseHleKernel -and (Test-Path -LiteralPath $probeFirmware)) {
    $probeFirmware = (Resolve-Path -LiteralPath $probeFirmware).Path
    $probeBackup = $probeFirmware + '.vitaarc-probe-disabled'
    $probeAllowedRoot = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build/vita3k')).Path + [IO.Path]::DirectorySeparatorChar
    if(-not $probeFirmware.StartsWith($probeAllowedRoot, [StringComparison]::OrdinalIgnoreCase) -or
       -not $probeBackup.StartsWith($probeAllowedRoot, [StringComparison]::OrdinalIgnoreCase)) {
      throw 'Resolved firmware paths are outside this project diagnostic directory.'
    }
    if(Test-Path -LiteralPath $probeBackup) {
      throw 'Preserve the existing firmware diagnostic backup before running this probe.'
    }
    $probeFirmwareHash = (Get-FileHash -LiteralPath $probeFirmware -Algorithm SHA256).Hash
    Move-Item -LiteralPath $probeFirmware -Destination $probeBackup
    $probeMovedFirmware = $true
  }
  $probeProcess = Start-Process -FilePath $probeExecutable -WorkingDirectory $probeRuntime `
    -ArgumentList "--frontend sdl --console --log-level 1 --keep-config --backend-renderer OpenGL --self $probeName-probe.self -r VARC61616" `
    -WindowStyle Hidden -PassThru
  Write-Output "$probeCodec probe PID $($probeProcess.Id); HLE kernel requested: $UseHleKernel"
  if(-not $probeProcess.WaitForExit(30000)) {
    throw "Probe remains live as PID $($probeProcess.Id); inspect it before another run."
  }
  $probeLog = Join-Path $probeOutput 'probe-vita3k.log'
  Copy-Item -LiteralPath (Join-Path $probeRuntime 'portable/vita3k.log') -Destination $probeLog
  $probeText = Get-Content -LiteralPath $probeLog -Raw
  if(-not $probeText.Contains("VitaArc $probeCodec probe: decode/position/seek/reset passed") -or
     -not $probeText.Contains("VitaArc $probeCodec probe: muted output worker/position/pause/seek/teardown passed") -or
     -not $probeText.Contains("VitaArc $probeCodec probe: exit=0")) {
    throw "Decoder probe did not pass; inspect $probeLog"
  }
  $probeText -split "`n" | Where-Object { $_.Contains("VitaArc $probeCodec probe:") } | Write-Output
} finally {
  if($probeMovedFirmware) {
    if(Test-Path -LiteralPath $probeFirmware) {
      throw "Firmware path changed during the probe; preserve backup at $probeBackup"
    }
    Move-Item -LiteralPath $probeBackup -Destination $probeFirmware
    if((Get-FileHash -LiteralPath $probeFirmware -Algorithm SHA256).Hash -ne $probeFirmwareHash) {
      throw 'Firmware restoration hash mismatch.'
    }
    Write-Output 'Original isolated firmware restored; SHA256 matches.'
  }
}
