"""Write THIRD-PARTY-NOTICES.txt for a release build (D6, LGPL obligations).

Studio's release builds link Qt statically. Qt's LGPL v3 then asks that
users get: the license texts, a notice that Qt is used, where its source is,
and a way to relink Studio with a changed Qt. This file covers the first
three and points to BUILDING.md for the fourth.

Inputs come from build_static_qt.py: <qt prefix>/share/flostudio/ holds
qt-build.json, qt-attributions.json and licenses/.

Usage: python3 tools/make_notices.py <qt prefix> <output file> [--commit SHA]
"""
import argparse
import json
from pathlib import Path

REPO = "https://github.com/hostkurd/flocms-studio"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("qt_prefix", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--commit", default="")
    args = parser.parse_args()

    share = args.qt_prefix / "share" / "flostudio"
    build = json.loads((share / "qt-build.json").read_text(encoding="utf-8"))
    attributions = json.loads((share / "qt-attributions.json").read_text(encoding="utf-8"))
    licenses = share / "licenses"

    version = build["version"]
    source = f"{REPO}/tree/{args.commit}" if args.commit else REPO
    lines = [
        "FloCMS Studio: third-party notices",
        "==================================",
        "",
        "FloCMS Studio is free software under the MIT License (see LICENSE).",
        "",
        f"It contains the Qt {version} libraries, linked statically, which are",
        "used under the GNU Lesser General Public License version 3 (LGPL v3).",
        "The full LGPL v3 and GPL v3 texts are at the end of this file.",
        "",
        f"Qt modules used: {', '.join(build['modules'])}.",
        f"Qt source code: {build['source']}/",
        f"FloCMS Studio source code for this build: {source}",
        "",
        "You may change Qt and relink FloCMS Studio with your changed version.",
        "BUILDING.md in the source code explains how to build FloCMS Studio",
        "against any Qt 6 build, including a static one.",
        "",
        "Qt itself contains third-party code. These are the components listed",
        "by the Qt modules above, with their licenses:",
        "",
    ]

    seen = set()
    for entry in sorted(attributions, key=lambda e: (e.get("Name", ""), e.get("Version", ""))):
        name = entry.get("Name", "").strip()
        if not name:
            continue
        key = (name, entry.get("Version", ""), entry.get("License", ""))
        if key in seen:
            continue
        seen.add(key)
        version_text = f" {entry['Version']}" if entry.get("Version") else ""
        lines.append(f"- {name}{version_text} ({entry.get('QtModule', '')})")
        if entry.get("License"):
            lines.append(f"  License: {entry['License']}")
        copyright_text = entry.get("Copyright", "")
        if isinstance(copyright_text, list):
            copyright_text = "; ".join(copyright_text)
        for line in str(copyright_text).splitlines():
            if line.strip():
                lines.append(f"  {line.strip()}")
        if entry.get("Homepage"):
            lines.append(f"  {entry['Homepage']}")
    lines.append("")

    for name in ("LGPL-3.0-only.txt", "GPL-3.0-only.txt"):
        text = licenses / name
        if not text.exists():
            raise SystemExit(f"missing license text: {text}")
        lines += ["", "=" * 72, name.removesuffix(".txt"), "=" * 72, "", text.read_text(encoding="utf-8").rstrip(), ""]

    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {args.output} ({len(seen)} third-party components)")


if __name__ == "__main__":
    main()
