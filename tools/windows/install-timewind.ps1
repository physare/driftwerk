# One-time setup (run as Administrator):
#  1. Links C:\Program Files\Common Files\VST3\Timewind.vst3 to %LOCALAPPDATA%\Timewind\Timewind.vst3
#     so updates never need admin rights again.
#  2. Registers a scheduled task that checks for new builds at logon and every 30 minutes.
#  3. Installs the current build right away.
# Needs the GitHub CLI signed in (run "gh auth login" once if it isn't).
$ErrorActionPreference = 'Stop'
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { Write-Host 'Please run this from a PowerShell window opened with "Run as administrator".' -ForegroundColor Yellow; exit 1 }
if (Get-Process -Name 'Ableton Live*' -ErrorAction SilentlyContinue) { Write-Host 'Please close Ableton first (it locks the plugin), then run this again.' -ForegroundColor Yellow; exit 1 }
$user      = [Environment]::UserName
$localApp  = [Environment]::GetFolderPath('LocalApplicationData')
$home_     = Join-Path $localApp 'Timewind'
New-Item -ItemType Directory -Force $home_ | Out-Null

# Clean up the old Driftwerk install (same plugin, old name).
Unregister-ScheduledTask -TaskName 'Driftwerk Updater' -Confirm:$false -ErrorAction SilentlyContinue
$old = 'C:\Program Files\Common Files\VST3\Driftwerk.vst3'
if (Test-Path $old) { cmd /c rmdir /s /q "`"$old`"" | Out-Null; if (Test-Path $old) { Remove-Item -Recurse -Force $old } }

$updater = Join-Path $home_ 'update-timewind.ps1'
Copy-Item -Force (Join-Path $PSScriptRoot 'update-timewind.ps1') $updater

# Install the current build first so the link target exists.
& powershell -NoProfile -ExecutionPolicy Bypass -File $updater

$vst3Dir = 'C:\Program Files\Common Files\VST3'
New-Item -ItemType Directory -Force $vst3Dir | Out-Null
$link = Join-Path $vst3Dir 'Timewind.vst3'
if (Test-Path $link) { Remove-Item -Recurse -Force $link }
New-Item -ItemType Junction -Path $link -Target (Join-Path $home_ 'Timewind.vst3') | Out-Null

$action   = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$updater`""
$logon    = New-ScheduledTaskTrigger -AtLogOn -User $user
$repeat   = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(1) -RepetitionInterval (New-TimeSpan -Minutes 30)
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable
$principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited
Register-ScheduledTask -TaskName 'Timewind Updater' -Action $action -Trigger @($logon, $repeat) -Settings $settings -Principal $principal -Force | Out-Null

Write-Host "Timewind installed. VST3 linked at $link, updates every 30 minutes (log: $home_\update.log)."
