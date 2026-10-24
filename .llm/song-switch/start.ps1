param([string]$Label = 'baseline', [string]$Self = 'profile.self')
$ErrorActionPreference = 'Stop'
$switchRoot = 'M:/Projects/PSV Projects/VitaArc/build/song-switch-20261025'
if(Get-Process -Name Vita3K,Vita3K-gl44 -ErrorAction SilentlyContinue) {
    throw 'Preserve the active emulator.'
}

$switchConfig = Join-Path $switchRoot 'config.yml'
$switchArgs = '--frontend sdl --console --log-level 2 --keep-config --load-config --config-location "' + $switchConfig + '" --backend-renderer OpenGL --self ' + $Self + ' -r VARC61616'
$switchChild = Start-Process -FilePath (Join-Path $switchRoot 'Vita3K-gl44.exe') `
    -WorkingDirectory $switchRoot -WindowStyle Hidden -PassThru -ArgumentList $switchArgs `
    -RedirectStandardOutput (Join-Path $switchRoot ($Label + '-stdout.log')) `
    -RedirectStandardError (Join-Path $switchRoot ($Label + '-stderr.log'))
@{pid=$switchChild.Id;started=[DateTime]::UtcNow.ToString('o');config=$switchConfig} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $switchRoot ($Label + '-process.json'))
Write-Output "Song-switch process: $($switchChild.Id)"
