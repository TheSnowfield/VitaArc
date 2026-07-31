$ErrorActionPreference = 'Stop'
$probeRoot = Split-Path -Parent $PSScriptRoot
$probeRuntime = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build/vita3k/runtime')).Path
$probeOutput = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build/vita-unwind-probe')).Path
$probeApp = Join-Path $probeRuntime 'portable/fs/ux0/app/VARC61616'
if(Get-Process -Name 'Vita3K', 'Vita3K-gl44' -ErrorAction SilentlyContinue) {
  throw 'An emulator is already running; preserve that session.'
}
foreach($probeFile in @('unwind-probe.self', 'unwind-fixture.so')) {
  Copy-Item -LiteralPath (Join-Path $probeOutput $probeFile) -Destination (Join-Path $probeApp $probeFile)
}
$probeProcess = Start-Process -FilePath (Join-Path $probeRuntime 'Vita3K.exe') `
  -WorkingDirectory $probeRuntime -WindowStyle Hidden -PassThru `
  -ArgumentList '--frontend sdl --console --log-level 1 --keep-config --backend-renderer OpenGL --self unwind-probe.self -r VARC61616'
Write-Output "Unwind probe PID $($probeProcess.Id)"
if(-not $probeProcess.WaitForExit(30000)) {
  throw "Probe remains live as PID $($probeProcess.Id); inspect it before another run."
}
$probeLog = Join-Path $probeOutput 'probe-vita3k.log'
Copy-Item -LiteralPath (Join-Path $probeRuntime 'portable/vita3k.log') -Destination $probeLog
$probeText = Get-Content -LiteralPath $probeLog -Raw
if(-not $probeText.Contains('VitaArc unwind probe: catch result=43') -or
   -not $probeText.Contains('VitaArc unwind probe: exit=0') -or
   -not $probeText.Contains('The app exited with code 0') -or
   $probeText.Contains('exception_handler')) {
  throw "Unwind probe did not pass; inspect $probeLog"
}
$probeText -split "`n" | Where-Object { $_.Contains('VitaArc unwind probe:') } | Write-Output
