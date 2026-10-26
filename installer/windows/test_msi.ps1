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

# Start-Process joins the arguments as they are: each one is quoted here
function Run-Msi([string[]] $arguments, [string] $log) {
    $all = @($arguments + @('/qn', '/norestart', '/l*v', $log) | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } })
    $p = Start-Process msiexec.exe -ArgumentList $all -Wait -PassThru
    # 3010 and 1641: done, Windows wants a restart (not for a per-user
    # install, but not a failure either)
    if ($p.ExitCode -notin 0, 3010, 1641) {
        # Job logs can't always be read from outside GitHub: what led to the
        # failure, as annotations
        $lines = @(Get-Content $log -ErrorAction SilentlyContinue)
        $at = [array]::FindIndex([string[]] $lines, [Predicate[string]] { param($l) $l -match 'Return value 3' })
        $from = if ($at -ge 0) { [Math]::Max(0, $at - 12) } else { [Math]::Max(0, $lines.Count - 12) }
        $lines[$from..([Math]::Min($lines.Count - 1, $from + 13))] | Where-Object { $_ } | ForEach-Object { Write-Output "::error::msi log: $($_.Substring(0, [Math]::Min(300, $_.Length)))" }
        Fail "msiexec $($arguments -join ' ') ended with $($p.ExitCode)"
    }
}
# What Windows Installer has installed with Studio's UpgradeCode (per-user
# installs aren't under HKCU's Uninstall key): product code and version
$installer = New-Object -ComObject WindowsInstaller.Installer
function Installed {
    $codes = $installer.RelatedProducts('{16604886-7502-402D-879A-8B0940B19498}')
    @(for ($i = 0; $i -lt $codes.Count; $i++) {
        $code = $codes.Item($i)
        [pscustomobject] @{ Code = $code; Version = $installer.ProductInfo($code, 'VersionString'); Name = $installer.ProductInfo($code, 'ProductName') }
    })
}

$folder = Join-Path $env:LOCALAPPDATA 'Programs\FloCMS Studio'
$shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'FloCMS Studio.lnk'
if ((Installed).Count -ne 0 -or (Test-Path $folder)) { Fail "Studio is already installed on this runner" }

Run-Msi @('/i', (Resolve-Path $Older).Path) 'install-older.log'
Run-Msi @('/i', (Resolve-Path $Msi).Path) 'install.log'
$entries = Installed
if ($entries.Count -ne 1) { Fail "$($entries.Count) FloCMS Studio entries in Settings > Apps after the upgrade, not 1" }
if ($entries[0].Version -ne $Version -or $entries[0].Name -ne 'FloCMS Studio') { Fail "installed: $($entries[0].Name) $($entries[0].Version), not FloCMS Studio $Version" }
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
$uninstall = $entries[0].Code
Run-Msi @('/x', $uninstall) 'uninstall.log'
if ((Installed).Count -ne 0) { Fail "Studio is still in Settings > Apps after uninstalling" }
if (Test-Path $shortcut) { Fail "uninstalling left the Start menu shortcut" }
$left = @(Get-ChildItem $folder -Recurse -Force -ErrorAction SilentlyContinue)
if ($left.Count -ne 0 -or (Test-Path $folder)) { Fail "uninstalling left $($left.Count) files in $folder" }
if (Test-Path 'HKCU:\Software\HostKurd\FloCMS Studio Installer') { Fail "uninstalling left the installer's registry key" }

Write-Output "::notice::installer: upgraded from the older version, $files files ($kib KiB) for the current user, shortcut and smoke test passed, uninstalled with nothing left"
