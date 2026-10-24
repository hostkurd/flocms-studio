#!/usr/bin/env bash
# The Linux test build (until the Linux release, D14): Studio with Qt and
# the FloStudio libraries in one folder, as an AppImage and as a .tar.gz of
# the same folder (for systems without FUSE: run its AppRun).
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
# The QML Studio uses, and only the platform plugins a desktop needs
export QML_SOURCES_PATHS="$root/app/qml"
export LD_LIBRARY_PATH="$appdir/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# xcb comes by itself; Wayland when this Qt has it; offscreen for the check below
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
tar -C "$work" -czf "$out/FloCMS-Studio-$version-linux-x86_64.tar.gz" FloCMS-Studio
echo "::notice::linux package: $(basename "$LDAI_OUTPUT") $(du -h "$LDAI_OUTPUT" | cut -f1), folder $(du -sh "$appdir" | cut -f1)"
