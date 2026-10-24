#!/usr/bin/env bash
# Every demo screen (app/demo/*.json) in English, Arabic and Kurdish, light
# and dark, as PNGs in <out>. A QML warning on any screen fails the step.
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
      "$studio" --demo "$demo" --language "$lang" --theme "$theme" --screenshot "$shot" > "$out/log.txt" 2>&1
      code=$?
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
