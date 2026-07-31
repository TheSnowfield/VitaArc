param([ValidateRange(60, 1800)][int]$TimeoutSeconds = 900, [string]$SeedData,
      [ValidateSet('touch-probe', 'sqlite-probe', 'lifecycle-probe', 'lifecycle-soak')]
      [string]$Program = 'touch-probe')
$ErrorActionPreference = 'Stop'
$probeRoot = Split-Path -Parent $PSScriptRoot
$probeOutput = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build/vita-touch-probe')).Path
$probeRuntime = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build/vita3k/runtime')).Path
if(Get-Process -Name 'Vita3K', 'Vita3K-gl44' -ErrorAction SilentlyContinue) {
  throw 'An emulator is already running; preserve that session.'
}
$probeUser = Join-Path $probeRuntime 'portable/fs/ux0/vitaarc'
$probeData = Join-Path $probeUser 'persistent'
$probeMailbox = Join-Path $probeUser 'touch-probe'
$probeSession = Join-Path $probeOutput ('run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$probeOriginal = Join-Path $probeSession 'original-persistent'
$probeFirmware = Join-Path $probeRuntime 'portable/fs/os0/kd/bootimage.skprx'
$probeBackup = $probeFirmware + '.vitaarc-touch-disabled'
$probeEboot = Join-Path $probeRuntime 'portable/fs/ux0/app/VARC61616/eboot.bin'
$probeConfig = Join-Path $probeRuntime 'touch-probe-config.yml'
$probeAllowed = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build')).Path + [IO.Path]::DirectorySeparatorChar
foreach($probePath in @($probeData, $probeMailbox, $probeSession, $probeOriginal, $probeFirmware, $probeBackup)) {
  $probeResolved = [IO.Path]::GetFullPath($probePath)
  if(-not $probeResolved.StartsWith($probeAllowed, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Probe path is outside the project build directory: $probeResolved"
  }
}
if($SeedData) {
  $SeedData = (Resolve-Path -LiteralPath $SeedData).Path
  if(-not $SeedData.StartsWith($probeAllowed, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Seed data must be a prior test directory under build.'
  }
}
if((Test-Path -LiteralPath $probeMailbox) -or (Test-Path -LiteralPath $probeBackup) -or
   (Test-Path -LiteralPath $probeSession)) { throw 'Preserve existing probe evidence/backup.' }
$probeFirmwareHash = (Get-FileHash -LiteralPath $probeFirmware -Algorithm SHA256).Hash
$probeEbootHash = (Get-FileHash -LiteralPath $probeEboot -Algorithm SHA256).Hash
$probeMovedData = $false
$probeMovedFirmware = $false
$probeProcess = $null
New-Item -ItemType Directory -Path $probeSession | Out-Null
try {
  if(Test-Path -LiteralPath $probeData) {
    Move-Item -LiteralPath $probeData -Destination $probeOriginal
    $probeMovedData = $true
  }
  $probeSeed = if($SeedData) { $SeedData } elseif($probeMovedData) { $probeOriginal } else { $null }
  if($probeSeed) { Copy-Item -LiteralPath $probeSeed -Destination $probeData -Recurse }
  New-Item -ItemType Directory -Path $probeMailbox | Out-Null
  $probeNativeLog = Join-Path $probeUser 'boot.log'
  if(Test-Path -LiteralPath $probeNativeLog) {
    Move-Item -LiteralPath $probeNativeLog -Destination (Join-Path $probeSession 'before-native.log')
  }
  $probeConfigText = Get-Content -LiteralPath (Join-Path $probeRuntime 'shader-probe-config.yml') -Raw
  $probeConfigText = $probeConfigText -replace '(?m)^gdbstub:.*$', 'gdbstub: false'
  $probeConfigText = $probeConfigText -replace '(?m)^show-welcome:.*$', 'show-welcome: false'
  Set-Content -LiteralPath $probeConfig -Value $probeConfigText -Encoding utf8NoBOM
  $probeExecutable = Join-Path $probeRoot "build/vita-$Program/$Program.self"
  Copy-Item -LiteralPath $probeExecutable -Destination (Split-Path -Parent $probeEboot)
  Move-Item -LiteralPath $probeFirmware -Destination $probeBackup
  $probeMovedFirmware = $true
  $probeProcess = Start-Process -FilePath (Join-Path $probeRuntime 'Vita3K-gl44.exe') `
    -WorkingDirectory $probeRuntime -WindowStyle Hidden -PassThru `
    -ArgumentList "--frontend sdl --console --log-level 1 --keep-config --load-config --config-location touch-probe-config.yml --backend-renderer OpenGL --self $Program.self -r VARC61616"
  $probeProcess.Id | Set-Content -LiteralPath (Join-Path $probeSession 'pid.txt')
  Write-Output "Native $Program PID $($probeProcess.Id); session $probeSession"
  $probeDeadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
  $probeLiveLog = Join-Path $probeRuntime 'portable/vita3k.log'
  while(-not $probeProcess.WaitForExit(250)) {
    if((Test-Path -LiteralPath $probeLiveLog) -and (Get-Item -LiteralPath $probeLiveLog).Length -gt 64MB) {
      Write-Output 'Stopping this diagnostic process after excessive log growth; retain first-fault evidence.'
      Stop-Process -Id $probeProcess.Id
      $probeProcess.WaitForExit()
      break
    }
    if([DateTime]::UtcNow -ge $probeDeadline) {
      throw "Probe remains live as PID $($probeProcess.Id); original data retained at $probeOriginal. Stop it before restoring data."
    }
  }
  $probeLog = Join-Path $probeSession 'vita3k.log'
  Copy-Item -LiteralPath (Join-Path $probeRuntime 'portable/vita3k.log') -Destination $probeLog
  Copy-Item -LiteralPath (Join-Path $probeUser 'boot.log') -Destination (Join-Path $probeSession 'native.log')
  # A faulty native loop can generate gigabytes of identical messages. Inspect
  # a bounded prefix, and retain the source log as evidence without loading it.
  $probeStream = [IO.File]::OpenRead($probeLog)
  try {
    $probeBytes = [byte[]]::new([int][Math]::Min($probeStream.Length, 16MB))
    $probeRead = $probeStream.Read($probeBytes, 0, $probeBytes.Length)
    $probeText = [Text.Encoding]::UTF8.GetString($probeBytes, 0, $probeRead)
  } finally { $probeStream.Dispose() }
  $probeMarker = 'VitaArc ' + $Program.Replace('-', ' ') + ': exit=0'
  if($Program.StartsWith('lifecycle-')) { $probeMarker = 'VitaArc lifecycle probe: exit=0' }
  if(-not $probeText.Contains($probeMarker) -or
     -not $probeText.Contains('The app exited with code 0') -or $probeText.Contains('exception_handler')) {
    throw "Probe did not exit cleanly; inspect $probeSession"
  }
  if($Program.StartsWith('lifecycle-') -and
     -not $probeText.Contains('VitaArc lifecycle probe: production unload completed')) {
    throw "Production unload did not complete; inspect $probeSession"
  }
  if($Program -eq 'lifecycle-soak' -and
     -not $probeText.Contains('VitaArc lifecycle probe: steady JNI counts passed')) {
    throw "Steady JNI counts did not pass; inspect $probeSession"
  }
  Write-Output "Native $Program exited cleanly. Judge gameplay from captured frames separately."
} finally {
  if($probeMovedFirmware) {
    if(Test-Path -LiteralPath $probeFirmware) { throw "Preserve firmware backup at $probeBackup" }
    Move-Item -LiteralPath $probeBackup -Destination $probeFirmware
  }
  if((Get-FileHash -LiteralPath $probeFirmware -Algorithm SHA256).Hash -ne $probeFirmwareHash -or
     (Get-FileHash -LiteralPath $probeEboot -Algorithm SHA256).Hash -ne $probeEbootHash) {
    throw 'Firmware/regular eboot hash mismatch.'
  }
  if(-not $probeProcess -or $probeProcess.HasExited) {
    if(Test-Path -LiteralPath $probeData) {
      Move-Item -LiteralPath $probeData -Destination (Join-Path $probeSession 'tested-persistent')
    }
    if($probeMovedData) { Move-Item -LiteralPath $probeOriginal -Destination $probeData }
    if(Test-Path -LiteralPath $probeMailbox) {
      Move-Item -LiteralPath $probeMailbox -Destination (Join-Path $probeSession 'frames')
    }
    Write-Output 'Original persistent directory restored; tested data and captures retained separately.'
  }
}
