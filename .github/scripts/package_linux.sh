#!/usr/bin/env bash
# The Linux builds: Studio with Qt and the FloStudio libraries in one
# folder, as
#  - a .deb for Ubuntu 24.04+ and Debian 13+: the folder in /opt/flocms-studio,
#    the `flocms-studio` command, a menu entry with Studio's icon; apt works
#    out the system libraries it needs (dpkg-shlibdeps)
#  - an AppImage (one file to run, nothing installed)
#  - a .tar.gz of the same folder (for systems without FUSE: run its AppRun)
# Each holds LICENSE.txt and THIRD-PARTY-NOTICES.txt in share/doc/flocms-studio.
# Usage: package_linux.sh <build folder> <out folder>
# Needs QMAKE (Qt's qmake) and downloads linuxdeploy and its Qt plugin.
set -euo pipefail
root=$PWD
build=$(realpath "$1"); out=$(mkdir -p "$2" && realpath "$2")
fail() { echo "::error::linux package: $*"; exit 1; }
work=$(mktemp -d)
tools="$work/tools"; mkdir -p "$tools"
for tool in linuxdeploy/linuxdeploy/linuxdeploy-x86_64.AppImage \
            linuxdeploy/linuxdeploy-plugin-qt/linuxdeploy-plugin-qt-x86_64.AppImage; do
  curl -fsSL -o "$tools/${tool##*/}" "https://github.com/${tool%/*}/releases/download/continuous/${tool##*/}" \
    || fail "could not download ${tool##*/}"
  chmod +x "$tools/${tool##*/}"
done
# No FUSE on the runners: the tools run unpacked
export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="$tools:$PATH"

appdir="$work/FloCMS-Studio"
mkdir -p "$appdir/usr/bin" "$appdir/usr/lib"
cp "$build/bin/flocms-studio" "$appdir/usr/bin/"
cp -P "$build"/libs/*/libFloStudio*.so* "$appdir/usr/lib/"
# The licenses, where the About page looks for them (../share/doc/flocms-studio)
doc="$appdir/usr/share/doc/flocms-studio"
mkdir -p "$doc"
cp "$root/LICENSE" "$doc/LICENSE.txt"
python3 "$root/tools/make_notices.py" "$doc/THIRD-PARTY-NOTICES.txt" --qt-version "$("$QMAKE" -query QT_VERSION)" \
  --commit "${GITHUB_SHA:-}" --qtkeychain "$build/_deps/qtkeychain-src/COPYING" > /dev/null || fail "notices"
# The QML Studio uses, and only the platform plugins a desktop needs
export QML_SOURCES_PATHS="$root/app/qml"
export LD_LIBRARY_PATH="$appdir/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# xcb comes by itself; Wayland when this Qt has it (CI installs qtwayland),
# so Wayland desktops don't fall back to X11; offscreen for the check below
plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)/platforms
extra="libqoffscreen.so"
for plugin in libqwayland-generic.so libqwayland-egl.so; do
  [ -f "$plugins/$plugin" ] && extra="$extra;$plugin"
done
export EXTRA_PLATFORM_PLUGINS="$extra"
version=$(QT_QPA_PLATFORM=offscreen "$build/bin/flocms-studio" --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+[^ ]*' || echo dev)
export LDAI_OUTPUT="$out/FloCMS-Studio-$version-x86_64.AppImage"
(cd "$work" && QMAKE="$QMAKE" linuxdeploy-x86_64.AppImage --appdir "$appdir" \
    -e "$appdir/usr/bin/flocms-studio" \
    -d "$root/app/linux/flocms-studio.desktop" \
    -i "$root/app/linux/flocms-studio.png" \
    --plugin qt --output appimage > "$work/deploy.log" 2>&1) \
  || { tail -30 "$work/deploy.log"; fail "linuxdeploy failed"; }
[ -f "$LDAI_OUTPUT" ] || fail "no AppImage was made"
# The folder runs on its own: no build folder, no Qt from elsewhere
(env -u LD_LIBRARY_PATH QT_QPA_PLATFORM=offscreen XDG_RUNTIME_DIR="$work" "$appdir/AppRun" --smoke-test) \
  || fail "the packaged Studio did not pass its smoke test"
if ls "$appdir"/usr/plugins/platforms/libqwayland-*.so > /dev/null 2>&1; then
  echo "::notice::linux package: Wayland platform plugins inside"
else
  echo "::warning::linux package: no Wayland platform plugin (this Qt has none): Wayland desktops use X11"
fi
tar -C "$work" -czf "$out/FloCMS-Studio-$version-linux-x86_64.tar.gz" FloCMS-Studio

# --- The .deb ---
case "$version" in [0-9]*) ;; *) fail "no version for the .deb ($version)";; esac
deb="$work/deb"
mkdir -p "$deb/DEBIAN" "$deb/opt/flocms-studio" "$deb/usr/bin" "$deb/usr/share/applications" "$deb/usr/share/doc/flocms-studio"
cp -a "$appdir"/. "$deb/opt/flocms-studio/"
# The command: the folder's AppRun, which sets up its Qt
cat > "$deb/usr/bin/flocms-studio" <<'SH'
#!/bin/sh
exec /opt/flocms-studio/AppRun "$@"
SH
chmod 755 "$deb/usr/bin/flocms-studio"
cp "$root/app/linux/flocms-studio.desktop" "$deb/usr/share/applications/flocms-studio.desktop"
for size in 64 128 256 512; do
  mkdir -p "$deb/usr/share/icons/hicolor/${size}x${size}/apps"
  cp "$root/app/icons/app-icon-$size.png" "$deb/usr/share/icons/hicolor/${size}x${size}/apps/flocms-studio.png"
done
cp "$root/LICENSE" "$deb/usr/share/doc/flocms-studio/copyright"
# The system libraries the folder's programs and libraries need (Qt and
# ours are inside it)
shlibs="$work/shlibs"; mkdir -p "$shlibs/debian"
printf 'Source: flocms-studio\n\nPackage: flocms-studio\nArchitecture: amd64\n' > "$shlibs/debian/control"
mapfile -t elfs < <(find "$deb/opt" -type f \( -name '*.so*' -o -perm -u+x \) -exec sh -c 'head -c 4 "$1" | grep -q "ELF" && echo "$1"' _ {} \;)
libdirs=$(find "$deb/opt" -type f -name '*.so*' -printf '%h\n' | sort -u | sed 's/^/-l/')
# shellcheck disable=SC2086
depends=$(cd "$shlibs" && dpkg-shlibdeps -O --ignore-missing-info $libdirs "${elfs[@]}" 2> "$work/shlibdeps.log" | sed -n 's/^shlibs:Depends=//p') \
  || { tail -20 "$work/shlibdeps.log"; fail "dpkg-shlibdeps failed"; }
[ -n "$depends" ] || { tail -20 "$work/shlibdeps.log"; fail "dpkg-shlibdeps found no dependencies"; }
cat > "$deb/DEBIAN/control" <<CONTROL
Package: flocms-studio
Version: $version
Architecture: amd64
Maintainer: HostKurd <https://github.com/hostkurd/flocms-studio/issues>
Installed-Size: $(du -sk --exclude=DEBIAN "$deb" | cut -f1)
Depends: $depends
Section: devel
Priority: optional
Homepage: https://github.com/hostkurd/flocms-studio
Description: create, run and change FloCMS projects
 FloCMS Studio is a desktop app for the FloCMS PHP framework: it creates
 projects, runs their website and a MariaDB server, and runs their
 migrations, checks, generators and Composer packages. Every action runs a
 real php flo or composer command and shows it.
CONTROL
debfile="$out/FloCMS-Studio-$version-amd64.deb"
dpkg-deb --build --root-owner-group -Zxz "$deb" "$debfile" > /dev/null || fail "dpkg-deb failed"
echo "::notice::linux package: $(basename "$debfile") $(du -h "$debfile" | cut -f1); Depends: $depends"
echo "::notice::linux package: $(basename "$LDAI_OUTPUT") $(du -h "$LDAI_OUTPUT" | cut -f1), folder $(du -sh "$appdir" | cut -f1)"
