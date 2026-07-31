param(
  [Parameter(Mandatory = $true)]
  [string]$CompilerPath
)

$ErrorActionPreference = 'Stop'
$probeRoot = Split-Path -Parent $PSScriptRoot
$probeRuntime = (Resolve-Path -LiteralPath (Join-Path $probeRoot 'build/vita3k/runtime')).Path
$probeOutput = Join-Path $probeRoot 'build/vita-shader-probe'
$probeApp = Join-Path $probeRuntime 'portable/fs/ux0/app/VARC61616'
$probeCompiler = (Resolve-Path -LiteralPath $CompilerPath).Path
if(Get-Process -Name 'Vita3K', 'Vita3K-gl44' -ErrorAction SilentlyContinue) {
  throw 'An emulator is already running; preserve that session.'
}
if(-not (Test-Path -LiteralPath (Join-Path $probeApp 'eboot.bin'))) {
  throw 'The isolated VitaArc installation is required.'
}
New-Item -ItemType Directory -Path $probeOutput -Force | Out-Null
foreach($probeStage in @('vert', 'frag')) {
  $probeProfile = if($probeStage -eq 'vert') { 'sce_vp_psp2' } else { 'sce_fp_psp2' }
  & $probeCompiler -profile $probeProfile `
    (Join-Path $probeRoot "resources/shaders/offline/textured_color.$probeStage.cg") `
    -o (Join-Path $probeOutput "probe.$probeStage.gxp")
  if($LASTEXITCODE -ne 0) { throw "Offline $probeStage compilation failed." }
}
Push-Location -LiteralPath $probeRoot
try {
  & wsl -d Ubuntu -- python3 tests/build_vita_shader_probe.py
  if($LASTEXITCODE -ne 0) { throw 'Shader probe SELF build failed.' }
} finally {
  Pop-Location
}
foreach($probeFile in @('shader-probe.self', 'probe.vert.gxp', 'probe.frag.gxp')) {
  Copy-Item -LiteralPath (Join-Path $probeOutput $probeFile) -Destination (Join-Path $probeApp $probeFile)
}

# Disable async pipeline skipping and enable readback in a separate config.
# --load-config is a flag; --config-location supplies the actual file path.
$probeDefaultConfig = Join-Path $probeRuntime 'portable/config.yml'
$probeDefaultHash = (Get-FileHash -LiteralPath $probeDefaultConfig -Algorithm SHA256).Hash
$probeConfig = Get-Content -LiteralPath $probeDefaultConfig -Raw
foreach($probeSetting in @('disable-surface-sync', 'async-pipeline-compilation')) {
  if($probeConfig -notmatch "(?m)^${probeSetting}: (true|false)\s*$") {
    throw "Expected configuration setting missing: $probeSetting"
  }
  $probeConfig = $probeConfig -replace "(?m)^${probeSetting}: (true|false)", "${probeSetting}: false"
}
[IO.File]::WriteAllText((Join-Path $probeRuntime 'shader-probe-config.yml'), $probeConfig)
$probeProcess = Start-Process -FilePath (Join-Path $probeRuntime 'Vita3K-gl44.exe') `
  -WorkingDirectory $probeRuntime -WindowStyle Hidden -PassThru `
  -ArgumentList '--frontend sdl --console --log-level 1 --keep-config --load-config --config-location shader-probe-config.yml --backend-renderer OpenGL --self shader-probe.self -r VARC61616'
Write-Output "Shader probe PID $($probeProcess.Id)"
if(-not $probeProcess.WaitForExit(30000)) {
  throw "Probe remains live as PID $($probeProcess.Id); inspect it before another run."
}
$probeLog = Join-Path $probeOutput 'probe-vita3k.log'
Copy-Item -LiteralPath (Join-Path $probeRuntime 'portable/vita3k.log') -Destination $probeLog
if((Get-FileHash -LiteralPath $probeDefaultConfig -Algorithm SHA256).Hash -ne $probeDefaultHash) {
  throw 'Default emulator configuration unexpectedly changed.'
}
$probeText = Get-Content -LiteralPath $probeLog -Raw
if(-not $probeText.Contains('VitaArc shader probe: linked=1') -or
   -not $probeText.Contains('VitaArc shader probe: exit=0') -or
   -not $probeText.Contains('The app exited with code 0') -or
   $probeText.Contains('exception_handler')) {
  throw "Shader probe did not pass; inspect $probeLog"
}
$probeText -split "`n" | Where-Object { $_.Contains('VitaArc shader probe:') } | Write-Output
Write-Output 'Default configuration preserved; SHA256 matches.'
