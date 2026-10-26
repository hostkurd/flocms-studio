#!/usr/bin/env bash
# Builds the Windows installer (studio.wxs) with WiX Toolset v5, then checks
# it with Windows Installer's own checks (ICE). Runs in Git Bash on Windows,
# as CI does; needs .NET (on GitHub's Windows runners) and Python.
# Usage: build_msi.sh <deployed folder with Studio.exe> <version> <out.msi>
set -euo pipefail
fail() { echo "::error::installer: $*"; exit 1; }
[ $# -eq 3 ] || fail "usage: build_msi.sh <folder> <version> <out.msi>"
here=$(cd "$(dirname "$0")" && pwd)
source=$(cd "$1" && pwd); version=$2; out=$3
[ -f "$source/Studio.exe" ] || fail "no Studio.exe in $source"
[ -f "$source/LICENSE.txt" ] || fail "no LICENSE.txt in $source"
# Windows Installer versions are three numbers, each up to 255.65535.65535
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "version $version is not major.minor.patch"

wix_version=5.0.2
export PATH="$PATH:$HOME/.dotnet/tools:$(cygpath -u "$USERPROFILE")/.dotnet/tools"
if ! wix --version 2> /dev/null | grep -q "^$wix_version"; then
  dotnet tool install --global wix --version "$wix_version" > /dev/null || fail "could not install WiX $wix_version"
fi
# Adding one that is already there is harmless; a missing one fails the build
for ext in WixToolset.UI.wixext WixToolset.Util.wixext; do
  wix extension add --global "$ext/$wix_version" > /dev/null 2>&1 || true
done

work=$(mktemp -d)
python "$here/license_rtf.py" "$source/LICENSE.txt" "$work/license.rtf"
win() { cygpath -w "$1"; }
wix build -arch x64 -nologo \
  -ext WixToolset.UI.wixext -ext WixToolset.Util.wixext \
  -d Version="$version" -d SourceDir="$(win "$source")" -d LicenseRtf="$(win "$work/license.rtf")" \
  -o "$(win "$PWD/$out")" "$(win "$here/studio.wxs")" > "$work/build.txt" 2>&1 || {
  # Job logs can't always be read from outside GitHub: WiX's messages as annotations
  grep -iE "error|warning" "$work/build.txt" | head -20 | while IFS= read -r line; do echo "::error::wix: $line"; done
  fail "wix build failed"
}
cat "$work/build.txt"
# The .wixpdb next to it is for debugging the package, not for shipping
rm -f "${out%.msi}.wixpdb"
wix msi validate -nologo -sice ICE38 -sice ICE64 -sice ICE91 "$(win "$PWD/$out")" > "$work/validate.txt" 2>&1 || {
  grep -iE "error|warning|ICE" "$work/validate.txt" | head -20 | while IFS= read -r line; do echo "::error::wix: $line"; done
  fail "the package failed Windows Installer's checks"
}
cat "$work/validate.txt"
rm -rf "$work"
