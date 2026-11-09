# Timewind auto-updater: installs the newest GitHub build into %LOCALAPPDATA%\Timewind.
# Runs from Task Scheduler. Skips while Ableton is open (the plugin file is locked then).
# The Timewind app can stay open: its files are renamed out of the way (Windows allows that
# for a running program) and the new version starts the next time you open it.
$ErrorActionPreference = 'Stop'
$repo    = 'physare/driftwerk'
$home_   = Join-Path $env:LOCALAPPDATA 'Timewind'
$current = Join-Path $home_ 'version.txt'
$log     = Join-Path $home_ 'update.log'
New-Item -ItemType Directory -Force $home_ | Out-Null
function Log($m) { "$(Get-Date -Format s)  $m" | Add-Content $log }

# Desktop and Start menu shortcuts for the Timewind app. Made once: delete them and they stay gone.
function Make-Shortcuts {
    try {
        $exe  = Join-Path $home_ 'Timewind.exe'
        $done = Join-Path $home_ 'shortcuts.done'
        if (-not (Test-Path $exe) -or (Test-Path $done)) { return }
        $shell = New-Object -ComObject WScript.Shell
        foreach ($dir in @([Environment]::GetFolderPath('Desktop'), [Environment]::GetFolderPath('Programs'))) {
            if (-not $dir) { continue }
            $lnk = $shell.CreateShortcut((Join-Path $dir 'Timewind.lnk'))
            $lnk.TargetPath = $exe
            $lnk.WorkingDirectory = $home_
            $lnk.IconLocation = "$exe,0"
            $lnk.Description = 'Timewind'
            $lnk.Save()
        }
        Set-Content $done 'yes'
        Log 'Made the Desktop and Start menu shortcuts'
    } catch { Log "Shortcuts failed: $_" }
}

# Copies a file over one that may be in use: if it is locked, the old one is renamed aside first.
function Replace-File($src, $dst) {
    try { Copy-Item -Force $src $dst; return } catch { }
    $aside = "$dst.old-" + [guid]::NewGuid().ToString('N').Substring(0, 8)
    Move-Item -Force $dst $aside
    Copy-Item -Force $src $dst
}

try {
    Make-Shortcuts
    # Old copies left by a previous update while the app was open (they go once it has closed).
    Get-ChildItem $home_ -Filter '*.old-*' -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
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
    $appOpen = [bool] (Get-Process -Name 'Timewind' -ErrorAction SilentlyContinue)
    Replace-File (Join-Path $unz 'Timewind.exe') (Join-Path $home_ 'Timewind.exe')
    foreach ($f in @('tw_onnxruntime.dll', 'hand_detector.onnx', 'hand_landmarks_detector.onnx')) {
        $src = Join-Path $unz $f
        if (Test-Path $src) { Replace-File $src (Join-Path $home_ $f) }
    }
    $self = Join-Path $unz 'update-timewind.ps1'
    if (Test-Path $self) { Copy-Item -Force $self (Join-Path $home_ 'update-timewind.ps1') }
    [IO.File]::WriteAllText($current, $remote)   # (Set-Content with -NoNewline and positional arguments misfires on Windows PowerShell 5.1)
    Remove-Item -Recurse -Force $tmp
    Log ("Updated to $remote" + $(if ($appOpen) { ' (the Timewind app was open: close and reopen it to use the new version)' } else { '' }))
    Make-Shortcuts
} catch {
    Log "Update failed: $_"
    exit 1
}
