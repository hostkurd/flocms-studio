"""Fill the unfinished messages of app/i18n/studio_{en,ar,ckb}.ts.

Run `cmake --build <build> --target update_translations` first, so the .ts
files hold the new source strings (marked unfinished). This fills only
those, from one or more strings files (tools/i18n/stageN.py: T for plain
strings, P for plurals), so what native readers changed in Linguist stays.
English gets plural forms only; its other strings show the source.

Usage: python3 tools/fill_translations.py tools/i18n/stage4.py [...]
Prints what is still missing and exits 1 if anything is.
"""
import importlib.util
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LANGUAGES = [("en", 0), ("ar", 1), ("ckb", 2)]


def load(paths):
    plain, plural = {}, {}
    for path in paths:
        spec = importlib.util.spec_from_file_location(Path(path).stem, path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        plain.update(getattr(module, "T", {}))
        plural.update(getattr(module, "P", {}))
    return plain, plural


def fill(path: Path, index: int, lang: str, plain: dict, plural: dict) -> list[str]:
    tree = ET.parse(path)
    missing = []
    for message in tree.getroot().iter("message"):
        source = message.find("source").text or ""
        translation = message.find("translation")
        if translation is None or translation.get("type") != "unfinished":
            continue
        if message.get("numerus") == "yes":
            if source not in plural:
                missing.append(source)
                continue
            for form in list(translation):
                translation.remove(form)
            translation.text = None
            for text in plural[source][index]:
                ET.SubElement(translation, "numerusform").text = text
            del translation.attrib["type"]
        elif lang != "en":
            if source not in plain:
                missing.append(source)
                continue
            translation.text = plain[source][index - 1]
            del translation.attrib["type"]
    tree.write(path, encoding="utf-8", xml_declaration=True)
    return missing


def main() -> int:
    plain, plural = load(sys.argv[1:])
    failed = False
    for lang, index in LANGUAGES:
        missing = fill(ROOT / "app" / "i18n" / f"studio_{lang}.ts", index, lang, plain, plural)
        print(f"{lang}: {len(missing)} missing")
        for source in missing:
            print(f"  {source!r}")
        failed |= bool(missing)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
