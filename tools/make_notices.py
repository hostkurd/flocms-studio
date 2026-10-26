"""Write THIRD-PARTY-NOTICES.txt for a release build (D6, LGPL obligations).

Studio uses Qt under the GNU LGPL v3. Qt's LGPL asks that users get the
license texts, a notice that Qt is used, and where its source is. Builds
ship Qt as DLLs / frameworks next to the program, which users can replace
with their own Qt build.

Qt's libraries contain third-party code (zlib, PCRE2, HarfBuzz, ...) whose
licenses also ask for their notices. licenses/qt-<version>-attributions.json
lists them, taken from the qt_attribution.json files of Qt's qtbase and
qtdeclarative sources (tests and examples left out), and licenses/qt/ holds
the license texts they refer to. Update both when Qt's version changes.
--attributions replaces the list (build_static_qt.py writes one).

The fonts built into Studio (app/fonts/LICENSES.txt, SIL OFL 1.1) and
QtKeychain (BSD-3-Clause, linked into Studio) are listed too.

--static: the release build, with Qt linked into the program (stage 9):
the notice says how to use your own Qt with it (build from source).

Usage:
  python3 tools/make_notices.py <output> --qt-version 6.12.0 [--commit SHA]
                                [--attributions qt-attributions.json]
                                [--static] [--qtkeychain <QtKeychain's COPYING>]
"""
import argparse
import re
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO = "https://github.com/hostkurd/flocms-studio"
QT_MODULES = "Qt Core, Gui, Qml, Quick, Quick Controls, Network, OpenGL and their plugins"


def license_ids(entry: dict) -> list[str]:
    text = str(entry.get("LicenseId", "")).replace("(", " ").replace(")", " ")
    return [word for word in text.split() if word not in ("AND", "OR", "WITH")]


def attribution_lines(path: Path) -> tuple[list[str], set[str]]:
    entries = json.loads(path.read_text(encoding="utf-8"))
    used: set[str] = set()
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
        if entry.get("License") or entry.get("LicenseId"):
            ids = f" ({entry['LicenseId']})" if entry.get("LicenseId") else ""
            lines.append(f"  License: {entry.get('License', '')}{ids}".rstrip())
        used.update(license_ids(entry))
        copyright_text = entry.get("Copyright", "")
        if isinstance(copyright_text, list):
            copyright_text = "; ".join(copyright_text)
        lines += [f"  {line.strip()}" for line in str(copyright_text).splitlines() if line.strip()]
        if entry.get("Homepage"):
            lines.append(f"  {entry['Homepage']}")
    return lines, used


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--qt-version", required=True)
    parser.add_argument("--commit", default="")
    parser.add_argument("--attributions", type=Path)
    parser.add_argument("--static", action="store_true")
    parser.add_argument("--qtkeychain", type=Path)
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
    ]
    if args.static:
        lines += [
            "Qt is linked into the FloCMS Studio program. You may change Qt and",
            "use your changed version with FloCMS Studio: build FloCMS Studio from",
            "the source code of this build (above) against your Qt, as BUILDING.md",
            "in the source code explains. Everything needed to do so is public.",
        ]
    else:
        lines += [
            "You may change Qt and use your changed version with FloCMS Studio:",
            "replace the Qt libraries that come with it, or build FloCMS Studio",
            "against your Qt as BUILDING.md in the source code explains.",
        ]
    attributions = args.attributions or ROOT / "licenses" / f"qt-{args.qt_version}-attributions.json"
    if not attributions.exists():
        raise SystemExit(f"missing {attributions}: list Qt {args.qt_version}'s third-party code first")
    attribution_text, used = attribution_lines(attributions)
    lines += attribution_text
    lines.append("")

    # Linked or built in besides Qt
    lines += ["", "Also in FloCMS Studio:", ""]
    if args.qtkeychain:
        if not args.qtkeychain.exists():
            raise SystemExit(f"missing {args.qtkeychain}: QtKeychain's license (build folder, _deps/qtkeychain-src)")
        pin = re.search(r"GIT_TAG \S+ # (\S+)", (ROOT / "libs" / "security" / "CMakeLists.txt").read_text(encoding="utf-8"))
        lines += [f"- QtKeychain {pin.group(1) if pin else ''} (https://github.com/frankosterfeld/qtkeychain), BSD-3-Clause:", "",
                  args.qtkeychain.read_text(encoding="utf-8", errors="replace").rstrip(), ""]
    fonts = ROOT / "app" / "fonts" / "LICENSES.txt"
    lines += ["- The fonts built into FloCMS Studio:", "", fonts.read_text(encoding="utf-8").rstrip(), ""]

    texts = [ROOT / "licenses" / "LGPL-3.0-only.txt", ROOT / "licenses" / "GPL-3.0-only.txt"]
    for license_id in sorted(used):
        text = ROOT / "licenses" / "qt" / f"{license_id}.txt"
        if license_id.startswith("LicenseRef-") and not text.exists():
            continue  # Qt's own reference ids; the copyright lines above carry them
        if not text.exists():
            raise SystemExit(f"missing license text: {text}")
        if text.name not in ("GPL-3.0-only.txt",):
            texts.append(text)
    for text in texts:
        body = text.read_text(encoding="utf-8").rstrip()
        lines += ["", "=" * 72, text.stem, "=" * 72, "", body, ""]

    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {args.output} ({len(used)} third-party license kinds)")


if __name__ == "__main__":
    main()
