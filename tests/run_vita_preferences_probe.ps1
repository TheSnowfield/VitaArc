$ErrorActionPreference = 'Stop'
$probeRoot = Split-Path -Parent $PSScriptRoot
$probeOutput = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build/vita-preferences-probe')).Path
$probeRuntime = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build/vita3k/runtime')).Path
$probeApp = Join-Path $probeRuntime 'portable/fs/ux0/app/VARC61616'
if(Get-Process -Name 'Vita3K', 'Vita3K-gl44' -ErrorAction SilentlyContinue) {
  throw 'An emulator is already running; preserve that session.'
}
$probePreferences = Join-Path $probeRuntime 'portable/fs/ux0/vitaarc/persistent/data/cocos-preferences.bin'
$probeBackup = Join-Path $probeOutput ('preferences-before-' + [Guid]::NewGuid().ToString('N') + '.bin')
$probeHadPreferences = Test-Path -LiteralPath $probePreferences
$probeHash = $null
if($probeHadPreferences) {
  $probeHash = (Get-FileHash -LiteralPath $probePreferences -Algorithm SHA256).Hash
  Copy-Item -LiteralPath $probePreferences -Destination $probeBackup
}
$probeProcess = $null
try {
  foreach($probeMode in @('write', 'read')) {
    $probeName = "preferences-$probeMode.self"
    Copy-Item -LiteralPath (Join-Path $probeOutput $probeName) -Destination $probeApp
    $probeProcess = Start-Process -FilePath (Join-Path $probeRuntime 'Vita3K.exe') `
      -WorkingDirectory $probeRuntime -WindowStyle Hidden -PassThru `
      -ArgumentList "--frontend sdl --console --log-level 1 --keep-config --backend-renderer OpenGL --self $probeName -r VARC61616"
    Write-Output "Native preferences $probeMode PID $($probeProcess.Id)"
    if(-not $probeProcess.WaitForExit(120000)) {
      throw "Probe remains live as PID $($probeProcess.Id); preserve it and the preference backup."
    }
    $probeLog = Join-Path $probeOutput "$probeMode-vita3k.log"
    Copy-Item -LiteralPath (Join-Path $probeRuntime 'portable/vita3k.log') -Destination $probeLog
    $probeText = [string](Get-Content -LiteralPath $probeLog -Raw)
    if(-not $probeText.Contains('VitaArc preferences probe: native UserDefault roundtrip passed') -or
       -not $probeText.Contains('VitaArc preferences probe: exit=0') -or
       -not $probeText.Contains('The app exited with code 0') -or $probeText.Contains('exception_handler')) {
      throw "Native preferences $probeMode failed; inspect $probeLog"
    }
    $probeText -split "`n" | Where-Object { $_.Contains('VitaArc preferences probe:') } | Write-Output
  }
} finally {
  if($probeProcess -and -not $probeProcess.HasExited) {
    Write-Output "Live probe preserved; preference backup retained at $probeBackup"
  } elseif($probeHadPreferences) {
    Copy-Item -LiteralPath $probeBackup -Destination $probePreferences
    if((Get-FileHash -LiteralPath $probePreferences -Algorithm SHA256).Hash -ne $probeHash) {
      throw 'Preference restoration SHA256 mismatch.'
    }
    Write-Output 'Original private preferences restored; SHA256 matches.'
  } elseif(Test-Path -LiteralPath $probePreferences) {
    # Only the exact file created by this test, under the isolated profile.
    Remove-Item -LiteralPath $probePreferences
  }
}
