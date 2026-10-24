"""Studio's icons from the logo (app/icons/logo.svg, from HostKurd):

- app/icons/logo-<n>.png   the window icon and the logo in the sidebar
- app/icons/studio.ico     Studio.exe and the installer (16 to 256 px)
- app/icons/studio.icns    the macOS app
- app/linux/flocms-studio.png   the AppImage (256 px)

Usage: python3 tools/make_icons.py   (needs cairosvg and Pillow)
"""
import io
from pathlib import Path

import cairosvg
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
ICONS = ROOT / "app" / "icons"
SVG = (ICONS / "logo.svg").read_bytes()


def render(size: int, margin: float = 0.06) -> Image.Image:
    """The logo centred on a transparent square, a small margin around it."""
    inner = round(size * (1 - 2 * margin))
    png = cairosvg.svg2png(bytestring=SVG, output_width=inner, output_height=inner)
    logo = Image.open(io.BytesIO(png)).convert("RGBA")
    square = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    square.paste(logo, ((size - inner) // 2, (size - inner) // 2), logo)
    return square


for size in (64, 128, 256, 512):
    render(size).save(ICONS / f"logo-{size}.png", optimize=True)
render(256).save(ICONS / "studio.ico", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
render(1024).save(ICONS / "studio.icns")
render(256).save(ROOT / "app" / "linux" / "flocms-studio.png", optimize=True)
print("icons written")
