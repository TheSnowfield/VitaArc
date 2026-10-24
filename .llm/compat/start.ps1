param([Parameter(Mandatory)][string]$Version, [string]$Self = 'probe.self')
$ErrorActionPreference = 'Stop'
$arcBase = [IO.Path]::GetFullPath('M:/Projects/PSV Projects/VitaArc/build/compat-current/')
$arcActive = Get-Process -Name Vita3K,Vita3K-gl44 -ErrorAction SilentlyContinue
if($arcActive | Where-Object { -not $_.Path.StartsWith($arcBase, [StringComparison]::OrdinalIgnoreCase) }) {
    throw 'Preserve the active emulator.'
}

$arcRoot = "M:/Projects/PSV Projects/VitaArc/build/compat-current/$Version"
if($arcActive | Where-Object { $_.Path -eq [IO.Path]::GetFullPath((Join-Path $arcRoot 'Vita3K-gl44.exe')) }) {
    throw 'This version is already running.'
}
$arcArgs = '--frontend sdl --console --log-level 2 --keep-config --load-config --config-location "' +
    (Join-Path $arcRoot 'config.yml') + '" --backend-renderer OpenGL --self ' + $Self + ' -r VARC61616'
$arcChild = Start-Process -FilePath (Join-Path $arcRoot 'Vita3K-gl44.exe') -WorkingDirectory $arcRoot `
    -WindowStyle Hidden -PassThru -ArgumentList $arcArgs `
    -RedirectStandardOutput (Join-Path $arcRoot 'stdout.log') `
    -RedirectStandardError (Join-Path $arcRoot 'stderr.log')
Write-Output $arcChild.Id
