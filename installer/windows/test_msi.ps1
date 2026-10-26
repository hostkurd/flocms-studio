# Tests the Windows installer the way a user meets it, without a window:
#  1. installs the previous version (the same files, a lower number)
#  2. installs this one over it: one Studio in Settings > Apps, this version
#  3. runs the installed Studio's smoke test, with only Windows on PATH
#  4. checks the Start menu shortcut
#  5. uninstalls from Settings > Apps' command: nothing of Studio is left
# Usage: test_msi.ps1 -Msi <this.msi> -Older <older.msi> -Version <x.y.z>
param(
    [Parameter(Mandatory)] [string] $Msi,
    [Parameter(Mandatory)] [string] $Older,
    [Parameter(Mandatory)] [string] $Version
)
$ErrorActionPreference = 'Stop'
function Fail([string] $why) { Write-Output "::error::installer: $why"; exit 1 }

function Run-Msi([string[]] $arguments, [string] $log) {
    $p = Start-Process msiexec.exe -ArgumentList ($arguments + @('/qn', '/norestart', '/l*v', $log)) -Wait -PassThru
    if ($p.ExitCode -ne 0) {
        Get-Content $log -ErrorAction SilentlyContinue | Select-String -Pattern 'error|failed|Return value 3' | Select-Object -Last 30
        Fail "msiexec $($arguments -join ' ') ended with $($p.ExitCode)"
    }
}
function Installed {
    @(Get-ChildItem 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall' -ErrorAction SilentlyContinue |
      Get-ItemProperty | Where-Object { $_.DisplayName -eq 'FloCMS Studio' })
}

$folder = Join-Path $env:LOCALAPPDATA 'Programs\FloCMS Studio'
$shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'FloCMS Studio.lnk'
if ((Installed).Count -ne 0 -or (Test-Path $folder)) { Fail "Studio is already installed on this runner" }

Run-Msi @('/i', (Resolve-Path $Older).Path) 'install-older.log'
Run-Msi @('/i', (Resolve-Path $Msi).Path) 'install.log'
$entries = Installed
if ($entries.Count -ne 1) { Fail "$($entries.Count) FloCMS Studio entries in Settings > Apps after the upgrade, not 1" }
# Windows Installer keeps a.b.c as the version it shows
if ($entries[0].DisplayVersion -ne $Version) { Fail "Settings > Apps shows $($entries[0].DisplayVersion), not $Version" }
if (-not (Test-Path (Join-Path $folder 'Studio.exe'))) { Fail "no Studio.exe in $folder" }
foreach ($file in 'LICENSE.txt', 'THIRD-PARTY-NOTICES.txt') {
    if (-not (Test-Path (Join-Path $folder $file))) { Fail "no $file in $folder" }
}
$files = @(Get-ChildItem $folder -Recurse -File).Count
$kib = [int]((Get-ChildItem $folder -Recurse -File | Measure-Object Length -Sum).Sum / 1024)

if (-not (Test-Path $shortcut)) { Fail "no Start menu shortcut at $shortcut" }
$target = (New-Object -ComObject WScript.Shell).CreateShortcut($shortcut).TargetPath
if ($target -ne (Join-Path $folder 'Studio.exe')) { Fail "the shortcut opens $target" }

$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
Remove-Item Env:QT_PLUGIN_PATH, Env:QML2_IMPORT_PATH, Env:QML_IMPORT_PATH -ErrorAction SilentlyContinue
$smoke = Start-Process (Join-Path $folder 'Studio.exe') -ArgumentList '--smoke-test' -Wait -PassThru -NoNewWindow
if ($smoke.ExitCode -ne 0) { Fail "the installed Studio's smoke test ended with $($smoke.ExitCode)" }

# What Settings > Apps runs to uninstall
$uninstall = $entries[0].PSChildName
Run-Msi @('/x', $uninstall) 'uninstall.log'
if ((Installed).Count -ne 0) { Fail "Studio is still in Settings > Apps after uninstalling" }
if (Test-Path $shortcut) { Fail "uninstalling left the Start menu shortcut" }
$left = @(Get-ChildItem $folder -Recurse -Force -ErrorAction SilentlyContinue)
if ($left.Count -ne 0 -or (Test-Path $folder)) { Fail "uninstalling left $($left.Count) files in $folder" }
if (Test-Path 'HKCU:\Software\HostKurd\FloCMS Studio Installer') { Fail "uninstalling left the installer's registry key" }

Write-Output "::notice::installer: upgraded from the older version, $files files ($kib KiB) for the current user, shortcut and smoke test passed, uninstalled with nothing left"
