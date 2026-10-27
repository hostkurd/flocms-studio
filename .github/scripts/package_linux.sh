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
# xcb comes by itself; Wayland from Qt's qtwayland archive (Qt 6.12 names
# the plugin libqwayland.so, earlier ones libqwayland-egl/-generic.so),
# so Wayland desktops don't fall back to X11; offscreen for the check below
plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)/platforms
extra="libqoffscreen.so"
for plugin in "$plugins"/libqwayland*.so; do
  [ -f "$plugin" ] && extra="$extra;$(basename "$plugin")"
done
case "$extra" in *qwayland*) ;; *) fail "this Qt has no Wayland platform plugin (its qtwayland archive)";; esac
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
# Wayland needs its platform plugin and the plugins it loads for windows
# (title bars, xdg-shell) and graphics; linuxdeploy-plugin-qt takes only
# the platform one, so the others are copied from Qt with what they need
qtplugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)
for kind in wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client; do
  [ -d "$qtplugins/$kind" ] || continue
  if [ ! -d "$appdir/usr/plugins/$kind" ]; then
    cp -r "$qtplugins/$kind" "$appdir/usr/plugins/"
    # Their own rpath, like the plugins linuxdeploy copied
    for so in "$appdir/usr/plugins/$kind"/*.so; do patchelf --set-rpath '$ORIGIN/../../lib' "$so" || fail "patchelf $so"; done
  fi
done
ls "$appdir"/usr/plugins/platforms/libqwayland*.so > /dev/null 2>&1 || fail "no Wayland platform plugin in the package"
[ -d "$appdir/usr/plugins/wayland-shell-integration" ] || fail "no Wayland shell integration in the package"
echo "::notice::linux package: Wayland plugins inside: $(cd "$appdir/usr/plugins" && ls -d platforms/libqwayland*.so wayland-* | tr '\n' ' ')"
tar -C "$work" -czf "$out/FloCMS-Studio-$version-linux-x86_64.tar.gz" FloCMS-Studio

# --- The .deb ---
case "$version" in [0-9]*) ;; *) fail "no version for the .deb ($version)";; esac
deb="$work/deb"
mkdir -p "$deb/DEBIAN" "$deb/opt/flocms-studio" "$deb/usr/bin" "$deb/usr/share/applications" "$deb/usr/share/doc/flocms-studio"
cp -a "$appdir"/. "$deb/opt/flocms-studio/"
# The system's own libraries come from apt (Depends below), not the copies
# linuxdeploy took from the build machine: a newer system's Mesa and glib
# must not meet older copies of them. Qt, ICU and ours stay.
find "$deb/opt/flocms-studio/usr/lib" -maxdepth 1 \( -type f -o -type l \) \
  ! -name 'libQt6*' ! -name 'libicu*' ! -name 'libFloStudio*' -delete
# linuxdeploy writes some files world-writable; installed as root they must not be
chmod -R u+rwX,go+rX,go-w "$deb"
# The command: the folder's AppRun, which sets up its Qt
cat > "$deb/usr/bin/flocms-studio" <<'SH'
#!/bin/sh
# Only Studio's own Qt (as CI tests it)
unset QT_PLUGIN_PATH QML2_IMPORT_PATH QML_IMPORT_PATH
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
# ours are inside it). DEBIAN/ must already be there: dpkg-shlibdeps then
# reads the $ORIGIN rpaths, else it picks the system's own Qt.
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
Depends: $depends, libssl3t64 | libssl3
Recommends: libsecret-1-0
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
