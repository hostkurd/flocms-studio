"""Write THIRD-PARTY-NOTICES.txt for a release build (D6, LGPL obligations).

Studio uses Qt under the GNU LGPL v3. Qt's LGPL asks that users get the
license texts, a notice that Qt is used, and where its source is. Builds
ship Qt as DLLs / frameworks next to the program, which users can replace
with their own Qt build; the single static program planned for the final
release (stage 9) adds the attributions of the code inside Qt
(--attributions, written by .github/scripts/build_static_qt.py).

Usage:
  python3 tools/make_notices.py <output> --qt-version 6.12.0 [--commit SHA]
                                [--attributions qt-attributions.json]
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO = "https://github.com/hostkurd/flocms-studio"
QT_MODULES = "Qt Core, Gui, Qml, Quick, Quick Controls, Network, OpenGL and their plugins"


def attribution_lines(path: Path) -> list[str]:
    entries = json.loads(path.read_text(encoding="utf-8"))
    lines = ["", "Qt itself contains third-party code. These are the components listed",
             "by the Qt modules above, with their licenses:", ""]
    seen = set()
    for entry in sorted(entries, key=lambda e: (e.get("Name", ""), e.get("Version", ""))):
        name = entry.get("Name", "").strip()
        key = (name, entry.get("Version", ""), entry.get("License", ""))
        if not name or key in seen:
            continue
        seen.add(key)
        version = f" {entry['Version']}" if entry.get("Version") else ""
        lines.append(f"- {name}{version} ({entry.get('QtModule', '')})")
        if entry.get("License"):
            lines.append(f"  License: {entry['License']}")
        copyright_text = entry.get("Copyright", "")
        if isinstance(copyright_text, list):
            copyright_text = "; ".join(copyright_text)
        lines += [f"  {line.strip()}" for line in str(copyright_text).splitlines() if line.strip()]
        if entry.get("Homepage"):
            lines.append(f"  {entry['Homepage']}")
    return lines


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--qt-version", required=True)
    parser.add_argument("--commit", default="")
    parser.add_argument("--attributions", type=Path)
    args = parser.parse_args()

    minor = ".".join(args.qt_version.split(".")[:2])
    qt_source = f"https://download.qt.io/official_releases/qt/{minor}/{args.qt_version}/submodules/"
    source = f"{REPO}/tree/{args.commit}" if args.commit else REPO
    lines = [
        "FloCMS Studio: third-party notices",
        "==================================",
        "",
        "FloCMS Studio is free software under the MIT License (see LICENSE.txt).",
        "",
        f"It uses the Qt {args.qt_version} libraries ({QT_MODULES}),",
        "under the GNU Lesser General Public License version 3 (LGPL v3).",
        "The full LGPL v3 and GPL v3 texts are at the end of this file.",
        "",
        f"Qt source code: {qt_source}",
        f"FloCMS Studio source code for this build: {source}",
        "",
        "You may change Qt and use your changed version with FloCMS Studio:",
        "replace the Qt libraries that come with it, or build FloCMS Studio",
        "against your Qt as BUILDING.md in the source code explains.",
    ]
    if args.attributions:
        lines += attribution_lines(args.attributions)
    lines.append("")

    for name in ("LGPL-3.0-only.txt", "GPL-3.0-only.txt"):
        text = (ROOT / "licenses" / name).read_text(encoding="utf-8").rstrip()
        lines += ["", "=" * 72, name.removesuffix(".txt"), "=" * 72, "", text, ""]

    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {args.output}")


if __name__ == "__main__":
    main()
