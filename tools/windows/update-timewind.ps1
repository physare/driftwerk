# Timewind auto-updater: installs the newest GitHub build into %LOCALAPPDATA%\Timewind.
# Runs from Task Scheduler. Skips while Ableton is open (the plugin file is locked then).
$ErrorActionPreference = 'Stop'
$repo    = 'physare/driftwerk'
$home_   = Join-Path $env:LOCALAPPDATA 'Timewind'
$current = Join-Path $home_ 'version.txt'
$log     = Join-Path $home_ 'update.log'
New-Item -ItemType Directory -Force $home_ | Out-Null
function Log($m) { "$(Get-Date -Format s)  $m" | Add-Content $log }

try {
    if (Get-Process -Name 'Ableton Live*' -ErrorAction SilentlyContinue) { Log 'Ableton is running, will try later'; exit 0 }

    $tmp = Join-Path $env:TEMP ('timewind-' + [guid]::NewGuid())
    New-Item -ItemType Directory $tmp | Out-Null
    gh release download latest --repo $repo --pattern version.txt --dir $tmp --clobber
    $remote = (Get-Content (Join-Path $tmp 'version.txt') -Raw).Trim()
    $local  = if (Test-Path $current) { (Get-Content $current -Raw).Trim() } else { '' }
    if ($remote -eq $local) { Remove-Item -Recurse -Force $tmp; exit 0 }

    gh release download latest --repo $repo --pattern Timewind-Windows.zip --dir $tmp --clobber
    $unz = Join-Path $tmp 'unz'
    Expand-Archive (Join-Path $tmp 'Timewind-Windows.zip') $unz -Force

    $vst = Join-Path $home_ 'Timewind.vst3'
    if (Test-Path $vst) { Remove-Item -Recurse -Force $vst }
    Copy-Item -Recurse (Join-Path $unz 'Timewind.vst3') $vst
    Copy-Item -Force (Join-Path $unz 'Timewind.exe') (Join-Path $home_ 'Timewind.exe')
    $self = Join-Path $unz 'update-timewind.ps1'
    if (Test-Path $self) { Copy-Item -Force $self (Join-Path $home_ 'update-timewind.ps1') }
    Set-Content -NoNewline $current $remote
    Remove-Item -Recurse -Force $tmp
    Log "Updated to $remote"
} catch {
    Log "Update failed: $_"
    exit 1
}
