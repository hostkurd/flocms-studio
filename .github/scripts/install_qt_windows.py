"""Install a Qt release for Windows from download.qt.io without the online installer.

aqtinstall 3.3.0 can't find Qt 6.12.0 for Windows: from 6.12 the repository
keeps one folder per compiler (windows_x86/desktop/qt6_6120/qt6_6120_msvc2022_64/)
instead of one shared folder. This reads that folder's Updates.xml (the Qt
installer framework's index) and unpacks the archives of the main package.

Usage: python install_qt_windows.py 6.12.0 win64_msvc2022_64 C:/Qt
Prints the Qt prefix (the folder with bin/ and lib/cmake/) on the last line.
"""
import hashlib
import subprocess
import sys
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

BASE = "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop"


def fetch(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=120) as response:
        return response.read()


def main() -> None:
    version, arch, out = sys.argv[1], sys.argv[2], Path(sys.argv[3])
    nodot = version.replace(".", "")
    compiler = arch.removeprefix("win64_")  # msvc2022_64
    folder = f"{BASE}/qt6_{nodot}/qt6_{nodot}_{compiler}"
    package = f"qt.qt6.{nodot}.{arch}"

    updates = ET.fromstring(fetch(f"{folder}/Updates.xml"))
    entry = next((p for p in updates.iter("PackageUpdate") if p.findtext("Name") == package), None)
    if entry is None:
        names = sorted(p.findtext("Name") for p in updates.iter("PackageUpdate"))
        sys.exit(f"{package} not in {folder}/Updates.xml; packages: {names[:20]}")

    package_version = entry.findtext("Version")
    archives = [a.strip() for a in (entry.findtext("DownloadableArchives") or "").split(",") if a.strip()]
    if not archives:
        sys.exit(f"{package} lists no archives")

    out.mkdir(parents=True, exist_ok=True)
    for archive in archives:
        url = f"{folder}/{package}/{package_version}{archive}"
        data = fetch(url)
        expected = fetch(url + ".sha1").decode().split()[0].strip()
        actual = hashlib.sha1(data).hexdigest()
        if actual != expected:
            sys.exit(f"checksum mismatch for {archive}: {actual} != {expected}")
        target = out / archive
        target.write_bytes(data)
        subprocess.run(["7z", "x", "-y", f"-o{out}", str(target)], check=True, stdout=subprocess.DEVNULL)
        target.unlink()
        print(f"unpacked {archive} ({len(data) // 1024} KiB)")

    # <prefix>/lib/cmake/Qt6/Qt6Config.cmake
    prefix = next((p.parents[3] for p in out.rglob("lib/cmake/Qt6/Qt6Config.cmake")), None)
    if prefix is None:
        sys.exit("Qt6Config.cmake not found after unpacking")
    print(prefix)


if __name__ == "__main__":
    main()
