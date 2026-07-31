param(
  [switch]$UseHleKernel,
  [switch]$Soak,
  [ValidateSet('Vita3K.exe', 'Vita3K-gl44.exe')]
  [string]$EmulatorName = 'Vita3K-gl44.exe'
)

$ErrorActionPreference = 'Stop'
$probeRoot = Split-Path -Parent $PSScriptRoot
$probeName = if($Soak) { 'lifecycle-soak' } else { 'lifecycle-probe' }
$probeOutput = (Resolve-Path -LiteralPath (Join-Path $probeRoot "build/vita-$probeName")).Path
$probeRuntime = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build/vita3k/runtime')).Path
$probeApp = Join-Path $probeRuntime 'portable/fs/ux0/app/VARC61616'
if(Get-Process -Name 'Vita3K', 'Vita3K-gl44' -ErrorAction SilentlyContinue) {
  throw 'An emulator is already running; preserve that session.'
}
Copy-Item -LiteralPath (Join-Path $probeOutput "$probeName.self") -Destination $probeApp
$probeEboot = Join-Path $probeApp 'eboot.bin'
$probeEbootHash = (Get-FileHash -LiteralPath $probeEboot -Algorithm SHA256).Hash
$probeFirmware = (Resolve-Path -LiteralPath (Join-Path $probeRuntime 'portable/fs/os0/kd/bootimage.skprx')).Path
$probeBackup = $probeFirmware + '.vitaarc-lifecycle-disabled'
$probeAllowedRoot = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build/vita3k')).Path + [IO.Path]::DirectorySeparatorChar
if(-not $probeFirmware.StartsWith($probeAllowedRoot, [StringComparison]::OrdinalIgnoreCase) -or
   -not $probeBackup.StartsWith($probeAllowedRoot, [StringComparison]::OrdinalIgnoreCase)) {
  throw 'Resolved firmware paths are outside the project diagnostic directory.'
}
$probeFirmwareHash = (Get-FileHash -LiteralPath $probeFirmware -Algorithm SHA256).Hash
$probeMovedFirmware = $false
try {
  if($UseHleKernel) {
    if(Test-Path -LiteralPath $probeBackup) { throw 'Preserve the existing firmware backup.' }
    Move-Item -LiteralPath $probeFirmware -Destination $probeBackup
    $probeMovedFirmware = $true
  }
  $probeProcess = Start-Process -FilePath (Join-Path $probeRuntime $EmulatorName) `
    -WorkingDirectory $probeRuntime -WindowStyle Hidden -PassThru `
    -ArgumentList "--frontend sdl --console --log-level 1 --keep-config --backend-renderer OpenGL --self $probeName.self -r VARC61616"
  Write-Output "Lifecycle probe PID $($probeProcess.Id); HLE kernel: $UseHleKernel"
  $probeTimeout = if($Soak) { 300000 } else { 180000 }
  if(-not $probeProcess.WaitForExit($probeTimeout)) {
    throw "Probe remains live as PID $($probeProcess.Id); inspect it before another run."
  }
  $probeLog = Join-Path $probeOutput 'probe-vita3k.log'
  Copy-Item -LiteralPath (Join-Path $probeRuntime 'portable/vita3k.log') -Destination $probeLog
  $probeText = [string](Get-Content -LiteralPath $probeLog -Raw)
  $probeMarkers = @('real game started', 'pause/resume passed', 'production unload completed', 'exit=0')
  if($Soak) { $probeMarkers += 'steady JNI counts passed' }
  foreach($probeMarker in $probeMarkers) {
    if(-not $probeText.Contains("VitaArc lifecycle probe: $probeMarker")) {
      throw "Lifecycle probe did not pass ($probeMarker); inspect $probeLog"
    }
  }
  if(-not $probeText.Contains('The app exited with code 0') -or $probeText.Contains('exception_handler')) {
    throw "Lifecycle probe did not exit cleanly; inspect $probeLog"
  }
  $probeText -split "`n" | Where-Object { $_.Contains('VitaArc lifecycle probe:') } | Write-Output
} finally {
  if($probeMovedFirmware) {
    if(Test-Path -LiteralPath $probeFirmware) { throw "Preserve firmware backup at $probeBackup" }
    Move-Item -LiteralPath $probeBackup -Destination $probeFirmware
  }
  if((Get-FileHash -LiteralPath $probeFirmware -Algorithm SHA256).Hash -ne $probeFirmwareHash) {
    throw 'Firmware restoration hash mismatch.'
  }
  if((Get-FileHash -LiteralPath $probeEboot -Algorithm SHA256).Hash -ne $probeEbootHash) {
    throw 'The regular game eboot changed during this probe.'
  }
  Write-Output 'Original firmware and regular eboot SHA256 match.'
}
