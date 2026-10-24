$ErrorActionPreference = 'Stop'
$profileRoot = 'M:/Projects/PSV Projects/VitaArc/build/cpu-profile-20261025'
$profileRuntime = $profileRoot
if(Get-Process -Name Vita3K,Vita3K-gl44 -ErrorAction SilentlyContinue) {
    throw 'Preserve the active emulator.'
}

$profileConfig = Join-Path $profileRoot 'config.yml'
$profileArgs = '--frontend sdl --console --log-level 2 --keep-config --load-config --config-location "' + $profileConfig + '" --backend-renderer OpenGL --self profile.self -r VARC61616'
$profileChild = Start-Process -FilePath (Join-Path $profileRuntime 'Vita3K-gl44.exe') `
    -WorkingDirectory $profileRuntime -WindowStyle Hidden -PassThru -ArgumentList $profileArgs `
    -RedirectStandardOutput (Join-Path $profileRoot 'baseline-stdout.log') `
    -RedirectStandardError (Join-Path $profileRoot 'baseline-stderr.log')
@{pid=$profileChild.Id;started=[DateTime]::UtcNow.ToString('o');config=$profileConfig} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $profileRoot 'baseline-process.json')
Write-Output "Profile process: $($profileChild.Id)"
