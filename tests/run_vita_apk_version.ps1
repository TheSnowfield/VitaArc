param(
  [Parameter(Mandatory)][ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+[a-z]?$')][string]$Version,
  [Parameter(Mandatory)][string]$Python,
  [string]$PackageDirectory = 'build/apk-versions',
  [string]$SelfOverride,
  [string]$SeedData,
  [ValidateSet('game','touch-probe','lifecycle-probe','lifecycle-soak')][string]$Program = 'game',
  [ValidateRange(10, 1200)][int]$ObserveSeconds = 40
)

# Run the actual packaged eboot with fresh, separate game data. Retain every
# variant and restore the private baseline installation even after a failure.
$ErrorActionPreference = 'Stop'
$versionRoot = Split-Path -Parent $PSScriptRoot
$versionBuild = (Resolve-Path -LiteralPath (Join-Path $versionRoot 'build')).Path
$versionOutput = (Resolve-Path -LiteralPath (Join-Path $versionRoot $PackageDirectory)).Path
if(-not $versionOutput.StartsWith($versionBuild + [IO.Path]::DirectorySeparatorChar,
    [StringComparison]::OrdinalIgnoreCase)) { throw 'Package directory must be under build.' }
$versionRuntime = Join-Path $versionBuild 'vita3k/runtime'
$versionVpk = Join-Path $versionOutput "VitaArc-$Version.vpk"
$versionSession = Join-Path $versionOutput ("run-$Version-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$versionStage = Join-Path $versionSession 'tested-app'
$versionApp = Join-Path $versionRuntime 'portable/fs/ux0/app/VARC61616'
$versionUser = Join-Path $versionRuntime 'portable/fs/ux0/vitaarc'
$versionOriginalApp = Join-Path $versionSession 'original-app'
$versionOriginalUser = Join-Path $versionSession 'original-user'
$versionFirmware = Join-Path $versionRuntime 'portable/fs/os0/kd/bootimage.skprx'
$versionBackup = $versionFirmware + '.vitaarc-version-disabled'
$versionDefaultConfig = Join-Path $versionRuntime 'portable/config.yml'
foreach($versionPath in @($versionStage, $versionApp, $versionUser, $versionOriginalApp,
                         $versionOriginalUser, $versionFirmware, $versionBackup)) {
  if(-not [IO.Path]::GetFullPath($versionPath).StartsWith($versionBuild + [IO.Path]::DirectorySeparatorChar,
      [StringComparison]::OrdinalIgnoreCase)) { throw "Path outside build: $versionPath" }
}
if(Get-Process -Name Vita3K,Vita3K-gl44 -ErrorAction SilentlyContinue) {
  throw 'Stop the previous emulator session before swapping an installation.'
}
if((Test-Path -LiteralPath $versionSession) -or (Test-Path -LiteralPath $versionBackup)) {
  throw 'Preserve the existing version-test evidence and firmware backup.'
}
function Get-VersionUserSnapshot {
  if(-not (Test-Path -LiteralPath $versionUser)) { return 'absent' }
  return ((Get-ChildItem -LiteralPath $versionUser -Recurse -File |
    Sort-Object FullName | ForEach-Object {
      $_.FullName.Substring($versionUser.Length) + ':' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }) -join "`n")
}
$versionOriginalSnapshot = Get-VersionUserSnapshot
if($SeedData) {
  $versionSeed = (Resolve-Path -LiteralPath (Join-Path $versionRoot $SeedData)).Path
  if(-not $versionSeed.StartsWith($versionBuild + [IO.Path]::DirectorySeparatorChar,
      [StringComparison]::OrdinalIgnoreCase)) { throw 'Seed data must be under build.' }
}
$versionHashes = @{}
foreach($versionPath in @($versionFirmware, $versionDefaultConfig,
                         (Join-Path $versionApp 'eboot.bin'),
                         (Join-Path $versionApp 'package/lib/armeabi-v7a/libcocos2dcpp.so'))) {
  $versionHashes[$versionPath] = (Get-FileHash -LiteralPath $versionPath -Algorithm SHA256).Hash
}
New-Item -ItemType Directory -Path $versionSession | Out-Null
& $Python -m zipfile -e $versionVpk $versionStage
if($LASTEXITCODE -ne 0) { throw 'Variant extraction failed; original installation is untouched.' }
if($SelfOverride) {
  $versionOverride = (Resolve-Path -LiteralPath (Join-Path $versionRoot $SelfOverride)).Path
  if(-not $versionOverride.StartsWith($versionBuild + [IO.Path]::DirectorySeparatorChar,
      [StringComparison]::OrdinalIgnoreCase)) { throw 'SELF override must be under build.' }
  Copy-Item -LiteralPath $versionOverride -Destination (Join-Path $versionStage 'eboot.bin')
}
$versionSelfName = 'eboot.bin'
if($Program -ne 'game') {
  $versionSelfName = "$Program.self"
  Copy-Item -LiteralPath (Join-Path $versionBuild "vita-$Program/$versionSelfName") -Destination $versionStage
}
$versionMovedApp = $false
$versionInstalled = $false
$versionMovedUser = $false
$versionOwnsUser = $false
$versionMovedFirmware = $false
$versionProcess = $null
$versionEvidence = [ordered]@{version=$Version; vpk=$versionVpk; session=$versionSession;
  self_sha256=(Get-FileHash -LiteralPath (Join-Path $versionStage 'eboot.bin') -Algorithm SHA256).Hash.ToLowerInvariant();
  fresh_data=(-not [bool]$SeedData); isolated_data=$true; hle_kernel=$true; renderer='OpenGL'; frontend='sdl';
  emulator='Vita3K-gl44.exe'; self_override=$SelfOverride; program=$Program; seed_data=$SeedData;
  executed_self_sha256=(Get-FileHash -LiteralPath (Join-Path $versionStage $versionSelfName) -Algorithm SHA256).Hash.ToLowerInvariant();
  physical_display_verified=$false; physical_touch_verified=$false}
try {
  Move-Item -LiteralPath $versionApp -Destination $versionOriginalApp
  $versionMovedApp = $true
  Move-Item -LiteralPath $versionStage -Destination $versionApp
  $versionInstalled = $true
  if(Test-Path -LiteralPath $versionUser) {
    Move-Item -LiteralPath $versionUser -Destination $versionOriginalUser
    $versionMovedUser = $true
  }
  $versionOwnsUser = $true
  New-Item -ItemType Directory -Path $versionUser | Out-Null
  if($SeedData) { Copy-Item -LiteralPath $versionSeed -Destination (Join-Path $versionUser 'persistent') -Recurse }
  New-Item -ItemType File -Path (Join-Path $versionUser 'capture-frame') | Out-Null
  if($Program -eq 'touch-probe') { New-Item -ItemType Directory -Path (Join-Path $versionUser 'touch-probe') | Out-Null }
  $versionConfig = Get-Content -LiteralPath (Join-Path $versionRuntime 'shader-probe-config.yml') -Raw
  $versionConfig = $versionConfig -replace '(?m)^gdbstub:.*$', 'gdbstub: false'
  $versionConfig = $versionConfig -replace '(?m)^show-welcome:.*$', 'show-welcome: false'
  Set-Content -LiteralPath (Join-Path $versionRuntime 'apk-version-config.yml') -Value $versionConfig -Encoding utf8NoBOM
  Move-Item -LiteralPath $versionFirmware -Destination $versionBackup
  $versionMovedFirmware = $true
  $versionArguments = '--frontend sdl --console --log-level 1 --keep-config --load-config --config-location apk-version-config.yml --backend-renderer OpenGL -r VARC61616'
  if($Program -ne 'game') { $versionArguments += " --self $versionSelfName" }
  $versionProcess = Start-Process -FilePath (Join-Path $versionRuntime 'Vita3K-gl44.exe') `
    -WorkingDirectory $versionRuntime -WindowStyle Hidden -PassThru `
    -ArgumentList $versionArguments
  $versionEvidence['pid'] = $versionProcess.Id
  $versionEvidence['started_utc'] = [DateTime]::UtcNow.ToString('o')
  $versionProcess.Id | Set-Content -LiteralPath (Join-Path $versionSession 'pid.txt')
  Write-Output "Testing packaged $Version eboot, PID $($versionProcess.Id), $versionSession"
  $versionDeadline = [DateTime]::UtcNow.AddSeconds($ObserveSeconds)
  $versionLiveLog = Join-Path $versionRuntime 'portable/vita3k.log'
  while(-not $versionProcess.WaitForExit(250)) {
    if((Test-Path -LiteralPath $versionLiveLog) -and (Get-Item -LiteralPath $versionLiveLog).Length -gt 64MB) {
      $versionEvidence['stopped_reason'] = 'Diagnostic exceeded 64 MiB log limit'
      break
    }
    if([DateTime]::UtcNow -ge $versionDeadline) {
      $versionEvidence['stopped_reason'] = 'Bounded diagnostic observation ended'
      break
    }
  }
  $versionEvidence['running_at_observation_end'] = -not $versionProcess.HasExited
  if(-not $versionProcess.HasExited) {
    # This process was created solely for this isolated, bounded diagnostic.
    Stop-Process -Id $versionProcess.Id
    $versionProcess.WaitForExit()
  } else { $versionEvidence['exit_code'] = $versionProcess.ExitCode }
  Copy-Item -LiteralPath $versionLiveLog -Destination (Join-Path $versionSession 'vita3k.log')
} finally {
  if($versionProcess -and -not $versionProcess.HasExited) {
    Stop-Process -Id $versionProcess.Id
    $versionProcess.WaitForExit()
  }
  if($versionMovedFirmware) { Move-Item -LiteralPath $versionBackup -Destination $versionFirmware }
  if($versionInstalled) { Move-Item -LiteralPath $versionApp -Destination $versionStage }
  if($versionMovedApp) { Move-Item -LiteralPath $versionOriginalApp -Destination $versionApp }
  if($versionOwnsUser -and (Test-Path -LiteralPath $versionUser)) {
    Move-Item -LiteralPath $versionUser -Destination (Join-Path $versionSession 'tested-user')
  }
  if($versionMovedUser) { Move-Item -LiteralPath $versionOriginalUser -Destination $versionUser }
  foreach($versionPath in $versionHashes.Keys) {
    if((Get-FileHash -LiteralPath $versionPath -Algorithm SHA256).Hash -ne $versionHashes[$versionPath]) {
      throw "Restoration hash mismatch: $versionPath"
    }
  }
  if((Get-VersionUserSnapshot) -ne $versionOriginalSnapshot) { throw 'Original game data snapshot differs.' }
  $versionEvidence['original_installation_and_data_restored'] = $true
  $versionEvidence | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $versionSession 'evidence.json') -Encoding utf8NoBOM
  Write-Output ($versionEvidence | ConvertTo-Json -Compress)
}
