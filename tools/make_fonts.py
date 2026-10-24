"""Make Studio's bundled fonts (D8) from the Google Fonts sources.

The fonts are variable fonts upstream. Studio bundles fixed weights cut
from them, because Qt picks weights from fixed font files the same way on
Windows, macOS and Linux. All four are under the SIL Open Font License 1.1
with no Reserved Font Name, so cut instances may keep their names.

Usage (needs fontTools: pip install fonttools):
  git clone --depth 1 --filter=blob:none --sparse https://github.com/google/fonts.git gfonts
  git -C gfonts sparse-checkout set ofl/instrumentsans ofl/bricolagegrotesque ofl/jetbrainsmono ofl/notosansarabic
  python3 tools/make_fonts.py gfonts
"""
import sys
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "app" / "fonts"

# (folder, variable file, file prefix, {style: axes}); the family name Qt
# sees is in FAMILY_NAMES.
FONTS = [
    ("instrumentsans", "InstrumentSans[wdth,wght].ttf", "InstrumentSans",
     {"Regular": {"wght": 400, "wdth": 100}, "Medium": {"wght": 500, "wdth": 100},
      "SemiBold": {"wght": 600, "wdth": 100}}),
    ("bricolagegrotesque", "BricolageGrotesque[opsz,wdth,wght].ttf", "BricolageGrotesque",
     {"SemiBold": {"wght": 600, "wdth": 100, "opsz": 24}, "Bold": {"wght": 700, "wdth": 100, "opsz": 24}}),
    ("jetbrainsmono", "JetBrainsMono[wght].ttf", "JetBrainsMono",
     {"Regular": {"wght": 400}, "Medium": {"wght": 500}}),
    ("notosansarabic", "NotoSansArabic[wdth,wght].ttf", "NotoSansArabic",
     {"Regular": {"wght": 400, "wdth": 100}, "Medium": {"wght": 500, "wdth": 100},
      "SemiBold": {"wght": 600, "wdth": 100}, "Bold": {"wght": 700, "wdth": 100}}),
]


FAMILY_NAMES = {"InstrumentSans": "Instrument Sans", "BricolageGrotesque": "Bricolage Grotesque",
                "JetBrainsMono": "JetBrains Mono", "NotoSansArabic": "Noto Sans Arabic"}
WEIGHTS = {"Regular": 400, "Medium": 500, "SemiBold": 600, "Bold": 700}


def name_font(font: TTFont, family: str, style: str) -> None:
    """One family with weights, the way the OpenType spec names them:
    ID 16/17 = typographic family and style; ID 1/2 = the four-style
    group (Regular/Bold keep the family, other weights get their own)."""
    names = font["name"]
    names.names = [n for n in names.names if n.nameID not in (1, 2, 3, 4, 6, 16, 17, 25)]
    legacy_family = family if style in ("Regular", "Bold") else f"{family} {style}"
    legacy_style = style if style in ("Regular", "Bold") else "Regular"
    postscript = f"{family.replace(' ', '')}-{style}"
    for name_id, value in [(1, legacy_family), (2, legacy_style), (3, f"FloCMS Studio cut: {postscript}"),
                           (4, f"{family} {style}"), (6, postscript), (16, family), (17, style)]:
        names.setName(value, name_id, 3, 1, 0x409)
    font["OS/2"].usWeightClass = WEIGHTS[style]
    bold = style == "Bold"
    font["OS/2"].fsSelection = (font["OS/2"].fsSelection & ~0b1100001) | (0b100000 if bold else 0b1000000)
    font["head"].macStyle = 1 if bold else 0


def main() -> int:
    source = Path(sys.argv[1]) / "ofl"
    OUT.mkdir(parents=True, exist_ok=True)
    licenses = []
    for folder, name, family, styles in FONTS:
        for style, axes in styles.items():
            font = TTFont(source / folder / name)
            static = instancer.instantiateVariableFont(font, axes)
            name_font(static, FAMILY_NAMES[family], style)
            target = OUT / f"{family}-{style}.ttf"
            static.save(target)
            print(f"{target.relative_to(ROOT)}  {target.stat().st_size // 1024} KiB")
        text = (source / folder / "OFL.txt").read_text(encoding="utf-8").strip()
        licenses.append(f"{family}\n{'=' * len(family)}\n\n{text}\n")
    (OUT / "LICENSES.txt").write_text(
        "The fonts in this folder are under the SIL Open Font License 1.1.\n"
        "Each was cut to fixed weights from its variable font with tools/make_fonts.py.\n\n"
        + "\n\n".join(licenses), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
