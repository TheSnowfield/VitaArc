param(
  [switch]$UseHleKernel,
  [switch]$EnableGdb,
  [switch]$EnableScreenshotKey,
  [ValidateSet('qt', 'sdl')][string]$Frontend = 'qt',
  [ValidateSet('Vita3K.exe', 'Vita3K-gl44.exe')][string]$EmulatorName = 'Vita3K.exe',
  [ValidateSet('OpenGL', 'Vulkan')][string]$Renderer = 'OpenGL',
  [ValidatePattern('^[0-9]{3}$')][string]$RunLabel = '030',
  [ValidateRange(1, 300)][int]$ObserveSeconds = 120
)

$ErrorActionPreference = 'Stop'
$gameRoot = Split-Path -Parent $PSScriptRoot
$gameOutput = (Resolve-Path -LiteralPath (Join-Path $gameRoot 'build/vita3k')).Path
$gameRuntime = (Resolve-Path -LiteralPath (Join-Path $gameOutput 'runtime')).Path
$gameApp = Join-Path $gameRuntime 'portable/fs/ux0/app/VARC61616'
$gameSelf = Join-Path $gameRoot 'build/VitaArc.self'
$gameConfig = Join-Path $gameRuntime "game-diagnostic-$RunLabel.yml"
$gameLog = Join-Path $gameRuntime 'portable/vita3k.log'
$gameSavedLog = Join-Path $gameOutput "boot-$RunLabel-vita3k.log"
$gameDefaultConfig = Join-Path $gameRuntime 'portable/config.yml'
$gameNativeLog = Join-Path $gameRuntime 'portable/fs/ux0/vitaarc/boot.log'
if(Get-Process -Name 'Vita3K', 'Vita3K-gl44' -ErrorAction SilentlyContinue) {
  throw 'An emulator is already running; preserve that session.'
}
if(Test-Path -LiteralPath $gameSavedLog) {
  throw "Preserve the existing run $RunLabel evidence before reusing its label."
}
if(-not (Test-Path -LiteralPath (Join-Path $gameApp 'package/lib/armeabi-v7a/libcocos2dcpp.so'))) {
  throw 'The isolated VitaArc installation must contain the local game library.'
}
$gameDefaultHash = (Get-FileHash -LiteralPath $gameDefaultConfig -Algorithm SHA256).Hash
$gameConfigText = Get-Content -LiteralPath (Join-Path $gameRuntime 'shader-probe-config.yml') -Raw
$gameConfigText = $gameConfigText -replace '(?m)^gdbstub:.*$', ('gdbstub: ' + $EnableGdb.IsPresent.ToString().ToLowerInvariant())
$gameConfigText = $gameConfigText -replace '(?m)^show-welcome:.*$', 'show-welcome: false'
# Vita3K merges only values differing from its compiled defaults. Its default
# "Vulkan" otherwise fails to override an existing global OpenGL setting.
# Backend selection is case-insensitive; retain an explicit non-default spelling.
$gameBackendSetting = if($Renderer -eq 'Vulkan') { 'VULKAN' } else { 'OpenGL' }
$gameConfigText = $gameConfigText -replace '(?m)^backend-renderer:.*$', ('backend-renderer: ' + $gameBackendSetting)
if($EnableScreenshotKey) {
  $gameConfigText = $gameConfigText -replace '(?m)^keyboard-take-screenshot:.*$', 'keyboard-take-screenshot: F12'
}
Set-Content -LiteralPath $gameConfig -Value $gameConfigText -Encoding utf8NoBOM
Copy-Item -LiteralPath $gameSelf -Destination (Join-Path $gameApp 'eboot.bin')
$gameSelfHash = (Get-FileHash -LiteralPath (Join-Path $gameApp 'eboot.bin') -Algorithm SHA256).Hash.ToLowerInvariant()

$gameFirmware = (Resolve-Path -LiteralPath (Join-Path $gameRuntime 'portable/fs/os0/kd/bootimage.skprx')).Path
$gameBackup = $gameFirmware + '.vitaarc-game-disabled'
$gameAllowedRoot = $gameOutput + [IO.Path]::DirectorySeparatorChar
if(-not $gameFirmware.StartsWith($gameAllowedRoot, [StringComparison]::OrdinalIgnoreCase) -or
   -not $gameBackup.StartsWith($gameAllowedRoot, [StringComparison]::OrdinalIgnoreCase)) {
  throw 'Resolved firmware paths are outside the project diagnostic directory.'
}
$gameFirmwareHash = (Get-FileHash -LiteralPath $gameFirmware -Algorithm SHA256).Hash
$gameMovedFirmware = $false
try {
  if($UseHleKernel) {
    if(Test-Path -LiteralPath $gameBackup) {
      throw "Preserve the existing firmware backup: $gameBackup"
    }
    Move-Item -LiteralPath $gameFirmware -Destination $gameBackup
    $gameMovedFirmware = $true
  }
  # This is the interactive game window; keep it visible for input validation.
  # This emulator can retain an older tail despite the guest's O_TRUNC request.
  # Start with a new file so current evidence cannot include previous markers.
  if(Test-Path -LiteralPath $gameNativeLog) {
    $gamePreviousNativeLog = Join-Path $gameOutput "before-$RunLabel-native.log"
    if(Test-Path -LiteralPath $gamePreviousNativeLog) { throw 'Preserve the earlier native log.' }
    Move-Item -LiteralPath $gameNativeLog -Destination $gamePreviousNativeLog
  }
  $gameProcess = Start-Process -FilePath (Join-Path $gameRuntime $EmulatorName) `
    -WorkingDirectory $gameRuntime -WindowStyle Normal -PassThru `
    -ArgumentList "--frontend $Frontend --log-level 1 --keep-config --load-config --config-location game-diagnostic-$RunLabel.yml --backend-renderer $Renderer -r VARC61616"
  $gameProcess.Id | Set-Content -LiteralPath (Join-Path $gameOutput "boot-$RunLabel.pid")
  $gameStartedUtc = $gameProcess.StartTime.ToUniversalTime()
  Write-Output "Game run $RunLabel PID $($gameProcess.Id); HLE kernel: $UseHleKernel; GDB: $EnableGdb"
  if($EnableGdb) {
    Write-Output 'GDB starts suspended: connect to port 2159 and continue the guest before interpreting startup results.'
  }
  # A bounded observation never kills a live game or treats a timeout as exit.
  $gameProcess.WaitForExit($ObserveSeconds * 1000) | Out-Null
  $gameProcess.Refresh()
  Copy-Item -LiteralPath $gameLog -Destination $gameSavedLog
  $gameText = [string](Get-Content -LiteralPath $gameSavedLog -Raw)
  $gameRuntimeText = $gameText
  $gameRuntimeLogSource = 'emulator'
  if((Test-Path -LiteralPath $gameNativeLog) -and
     (Get-Item -LiteralPath $gameNativeLog).LastWriteTimeUtc -ge $gameStartedUtc) {
    $gameSavedNativeLog = Join-Path $gameOutput "boot-$RunLabel-native.log"
    Copy-Item -LiteralPath $gameNativeLog -Destination $gameSavedNativeLog
    # Qt buffers its own log; the current native file provides runtime markers.
    $gameRuntimeText = [string](Get-Content -LiteralPath $gameSavedNativeLog -Raw)
    $gameRuntimeLogSource = 'native'
  }
  $gameEvidence = [ordered]@{
    run = $RunLabel
    pid = $gameProcess.Id
    running = -not $gameProcess.HasExited
    hle_kernel_requested = $UseHleKernel.IsPresent
    gdb_requested = $EnableGdb.IsPresent
    screenshot_key_enabled = $EnableScreenshotKey.IsPresent
    frontend = $Frontend
    emulator = $EmulatorName
    renderer = $Renderer
    observed_renderer = $(if($gameText.Contains('[renderer::vulkan::')) { 'Vulkan' } elseif($gameText.Contains('[renderer::gl::create]')) { 'OpenGL' } else { 'Unknown' })
    sampled_at = [DateTime]::UtcNow.ToString('o')
    self_sha256 = $gameSelfHash
    runtime_log_source = $gameRuntimeLogSource
    cocos_initialized = $gameRuntimeText.Contains('Cocos initialized at 960x544')
    frame_120_presented = $gameRuntimeText.Contains('Presented Cocos frame 120')
    frame_1800_captured = $gameRuntimeText.Contains('Cocos frame 1800 capture saved')
    mp3_disabled = $gameRuntimeText.Contains('MP3 initialization failed;')
    loaded_bgm = [regex]::Matches($gameRuntimeText, 'Loaded BGM channel [0-9]+:').Count
    native_fault_reported = $gameText.Contains('exception_handler')
    successful_gameplay = $false
    audible_output_verified = $false
  }
  if($gameProcess.HasExited) {
    $gameEvidence['exit_code'] = $gameProcess.ExitCode
  }
  $gameEvidence | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $gameOutput "boot-$RunLabel-evidence.json")
  Write-Output ($gameEvidence | ConvertTo-Json -Compress)
} finally {
  if($gameMovedFirmware) {
    if(Test-Path -LiteralPath $gameFirmware) {
      throw "Firmware path changed; preserve backup at $gameBackup"
    }
    Move-Item -LiteralPath $gameBackup -Destination $gameFirmware
    if((Get-FileHash -LiteralPath $gameFirmware -Algorithm SHA256).Hash -ne $gameFirmwareHash) {
      throw 'Firmware restoration hash mismatch.'
    }
    Write-Output 'Original isolated firmware restored; SHA256 matches.'
  }
  if((Get-FileHash -LiteralPath $gameDefaultConfig -Algorithm SHA256).Hash -ne $gameDefaultHash) {
    throw 'Default emulator configuration changed during the run.'
  }
}
