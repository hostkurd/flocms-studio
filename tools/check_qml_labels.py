"""Check that every translated label in app/qml shows through `ui:` (D9).

An AppText (or Heading, SectionLabel) whose `text:` is a translation
(qsTr, Texts.*) would lose the right-to-left mark AppText adds to `ui`, and
an Arabic or Kurdish label starting with a Latin word ("PHP و ئامرازەکان")
would then read left to right. Names, paths and versions use `text:`.

Usage: python3 tools/check_qml_labels.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LABELS = {"AppText", "Heading", "SectionLabel"}
OPEN = re.compile(r"\b([A-Z]\w*)\s*\{")
TRANSLATED = re.compile(r"\bqsTr\(|\bTexts\.")


def check(path: Path) -> list[str]:
    problems = []
    stack: list[str] = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        code = line.split("//")[0]
        # Each element opened on this line, innermost last; then the text
        # on the line belongs to the innermost one still open
        opened = []
        for match in OPEN.finditer(code):
            opened.append(match.group(1))
        stack.extend(opened)
        current = stack[-1] if stack else ""
        text = re.search(r"\btext:\s*(.*)", code)
        if text and current in LABELS and TRANSLATED.search(text.group(1)):
            problems.append(f"{path.relative_to(ROOT)}:{number}: {current} shows a translation with `text:`; use `ui:`")
        for _ in range(code.count("}") - (code.count("{") - len(opened))):
            if stack:
                stack.pop()
    return problems


def main() -> int:
    problems = []
    for path in sorted((ROOT / "app" / "qml").glob("*.qml")):
        problems += check(path)
    for problem in problems:
        print(problem)
    print(f"{len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
