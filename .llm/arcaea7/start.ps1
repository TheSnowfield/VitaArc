param([string]$Label = 'boot')
$ErrorActionPreference = 'Stop'
$arcRoot = 'M:/Projects/PSV Projects/VitaArc/build/arcaea-7.0.260c/runtime'
$arcActive = Get-Process -Name Vita3K,Vita3K-gl44 -ErrorAction SilentlyContinue
$arcCompat = [IO.Path]::GetFullPath('M:/Projects/PSV Projects/VitaArc/build/compat-current/')
if($arcActive | Where-Object { -not $_.Path.StartsWith($arcCompat, [StringComparison]::OrdinalIgnoreCase) }) {
    throw 'Preserve the active emulator.'
}

$arcArgs = '--frontend sdl --console --log-level 2 --keep-config --load-config --config-location "' +
    (Join-Path $arcRoot 'config.yml') + '" --backend-renderer OpenGL --self probe.self -r VARC61616'
$arcChild = Start-Process -FilePath (Join-Path $arcRoot 'Vita3K-gl44.exe') -WorkingDirectory $arcRoot `
    -WindowStyle Hidden -PassThru -ArgumentList $arcArgs `
    -RedirectStandardOutput (Join-Path $arcRoot ($Label + '-stdout.log')) `
    -RedirectStandardError (Join-Path $arcRoot ($Label + '-stderr.log'))
Write-Output $arcChild.Id
