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
LABELS = {"AppText", "Heading", "SectionLabel", "Mono"}
# An element opening ("AppText {"), any other brace (a JS block, an
# object), or a `text:` binding, in the order they appear
TOKEN = re.compile(r"\b([A-Z]\w*)\s*\{|\{|\}|\btext:\s*([^;\n]*)")
TRANSLATED = re.compile(r"\bqsTr\(|\bTexts\.")


def check(path: Path) -> list[str]:
    problems = []
    stack: list[str] = []  # element names; "" for a brace that isn't one
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        # String contents blanked (same length), so braces and `//` inside
        # strings count for nothing; then the comment is cut off
        code = re.sub(r'"(?:\\.|[^"\\])*"', lambda m: '"' + "x" * (len(m.group(0)) - 2) + '"', line)
        if "//" in code:
            code = code[: code.index("//")]
        original = line
        for match in TOKEN.finditer(code):
            token = match.group(0)
            if match.group(2) is not None:
                current = stack[-1] if stack else ""
                value = original[match.start(2):match.end(2)]
                if current in LABELS and TRANSLATED.search(value):
                    problems.append(f"{path.relative_to(ROOT)}:{number}: {current} shows a translation with `text:`; use `ui:`")
            elif token == "}":
                if stack:
                    stack.pop()
            else:
                stack.append(match.group(1) or "")
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
