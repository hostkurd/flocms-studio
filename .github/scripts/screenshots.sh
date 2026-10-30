#!/usr/bin/env bash
# Every demo screen (app/demo/*.json) in English, Arabic and Kurdish, light
# and dark, as PNGs in <out>. A QML warning on any screen fails the step.
# Each screen gets 60 s (GNU timeout: the Linux job only).
# Usage: screenshots.sh <studio program> <out>
set -uo pipefail
studio=$1
out=$2
mkdir -p "$out"
failed=0
count=0
for demo in app/demo/*.json; do
  name=$(basename "$demo" .json)
  for lang in en ar ckb; do
    for theme in light dark; do
      shot="$out/$name-$lang-$theme.png"
      # One screen never takes a minute: a stuck one (seen once in the
      # v0.1.4 release run, never again) is tried once more, then named
      timeout 60 "$studio" --demo "$demo" --language "$lang" --theme "$theme" --screenshot "$shot" > "$out/log.txt" 2>&1
      code=$?
      if [ $code -eq 124 ]; then
        echo "::warning::screenshot $name $lang $theme took over 60 s; trying it once more"
        timeout 60 "$studio" --demo "$demo" --language "$lang" --theme "$theme" --screenshot "$shot" > "$out/log.txt" 2>&1
        code=$?
        [ $code -eq 124 ] && echo "::error::screenshot $name $lang $theme hung twice"
      fi
      warnings=$(grep -E "qrc:|\.qml|QML|Warning|warning" "$out/log.txt" || true)
      if [ $code -ne 0 ] || [ ! -s "$shot" ] || [ -n "$warnings" ]; then
        echo "::error::screenshot $name $lang $theme: exit $code ${warnings//$'\n'/ | }"
        failed=1
      fi
      count=$((count + 1))
    done
  done
done
rm -f "$out/log.txt"
echo "::notice::screenshots: $count taken, failed=$failed"
exit $failed
