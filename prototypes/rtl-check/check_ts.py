"""Checks the translation files: no hidden direction marks, nothing unfinished,
and every `code word` of the source kept in the translation.

Direction marks are added at display time only (Language::rich), so a mark
in a .ts file is always a mistake: invisible to translators and reviewers.

Usage: python3 check_ts.py i18n/*.ts
"""
import re
import sys
import xml.etree.ElementTree as ET

MARKS = {
    0x200E: "LRM", 0x200F: "RLM", 0x061C: "ALM",
    0x202A: "LRE", 0x202B: "RLE", 0x202C: "PDF", 0x202D: "LRO", 0x202E: "RLO",
    0x2066: "LRI", 0x2067: "RLI", 0x2068: "FSI", 0x2069: "PDI",
}
CODE = re.compile(r"`([^`]*)`")

problems = []
for path in sys.argv[1:]:
    with open(path, encoding="utf-8") as f:
        text = f.read()
    for line_no, line in enumerate(text.splitlines(), 1):
        for ch in line:
            if ord(ch) in MARKS:
                problems.append(f"{path}:{line_no}: hidden {MARKS[ord(ch)]} (U+{ord(ch):04X})")
    for message in ET.fromstring(text).iter("message"):
        source = message.findtext("source") or ""
        translation = message.find("translation")
        if translation is None or translation.get("type") in ("unfinished", "vanished", "obsolete"):
            problems.append(f"{path}: not finished: {source!r}")
            continue
        forms = [f.text or "" for f in translation.findall("numerusform")] or [translation.text or ""]
        for form in forms:
            if sorted(CODE.findall(form)) != sorted(CODE.findall(source)):
                problems.append(f"{path}: code words differ in {form!r} (source {source!r})")

for problem in problems:
    print(problem)
print(f"{len(sys.argv) - 1} file(s) checked, {len(problems)} problem(s)")
sys.exit(1 if problems else 0)
