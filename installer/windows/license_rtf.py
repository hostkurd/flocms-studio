"""Writes a text file (LICENSE.txt) as RTF, for the installer's license page
(WiX shows the license only from RTF).

Usage: python license_rtf.py <LICENSE.txt> <out.rtf>
"""
import sys


def rtf(text: str) -> str:
    out = []
    for ch in text.replace("\r\n", "\n"):
        if ch in "\\{}":
            out.append("\\" + ch)
        elif ch == "\n":
            out.append("\\par\n")
        elif ord(ch) > 127:
            # RTF \u takes a signed 16-bit number, then a fallback character
            code = ord(ch)
            if code > 0xFFFF:
                out.append("?")
                continue
            out.append("\\u%d?" % (code - 0x10000 if code > 0x7FFF else code))
        else:
            out.append(ch)
    return ("{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fswiss Segoe UI;}}\\fs18\n"
            + "".join(out) + "}\n")


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    with open(sys.argv[1], encoding="utf-8") as f:
        text = f.read()
    with open(sys.argv[2], "w", encoding="ascii", newline="\n") as f:
        f.write(rtf(text))
    return 0


if __name__ == "__main__":
    sys.exit(main())
