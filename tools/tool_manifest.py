"""Make the list of tools Studio may download (D15): libs/core/resources/tools.json.

For each tool it finds the newest files at the official sources, downloads
each one, and records its SHA-256 and size. Where the source publishes a
hash it must match (windows.php.net releases.json, getcomposer.org's
.sha256sum, MariaDB's sha256sums.txt); static-php-cli publishes none
(github.com/crazywhalecc/static-php-cli/issues/1245), so the hash recorded
is the one of the file downloaded here.

  python3 tools/tool_manifest.py            print the list it finds
  python3 tools/tool_manifest.py --write    and write libs/core/resources/tools.json
  python3 tools/tool_manifest.py --check    exit 1 when the committed list differs

Sources, all HTTPS:
  PHP, Windows       windows.php.net (NTS x64 zips; no ARM builds exist)
  PHP, macOS/Linux   dl.static-php.dev, the "common" builds (one php binary)
  Composer           getcomposer.org (composer.phar, run with the chosen PHP)
  MariaDB, Windows   archive.mariadb.org (the zip). None for macOS (MariaDB
                     builds none: Homebrew), none for Linux (the packages)
"""
import argparse
import hashlib
import json
import os
import re
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "libs" / "core" / "resources" / "tools.json"
PHP_MINORS = 2          # the two newest PHP versions (Q9)
MARIADB_SERIES = "11.8"  # the newest LTS
STATIC_PLATFORMS = {("macos", "aarch64"): "macos-arm64", ("macos", "x86_64"): "macos-x86_64",
                    ("linux", "x86_64"): "linux-x86_64", ("linux", "aarch64"): "linux-arm64"}
UA = {"User-Agent": "FloCMS Studio tool list (github.com/hostkurd/flocms-studio)"}


def notice(kind: str, text: str) -> None:
    if os.environ.get("GITHUB_ACTIONS"):
        print(f"::{kind}::" + text.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A"))
    else:
        print(f"{kind}: {text}")


def fetch(url: str) -> bytes:
    for attempt in range(3):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as response:
                if not response.geturl().startswith("https://"):
                    raise RuntimeError(f"{url} redirected to {response.geturl()}")
                return response.read()
        except Exception as error:  # noqa: BLE001 - retried, then reported
            if attempt == 2:
                raise RuntimeError(f"{url}: {error}") from error
            time.sleep(3)
    raise AssertionError


def measure(url: str) -> tuple[str, int]:
    """SHA-256 and size of the file at url, streamed."""
    digest = hashlib.sha256()
    size = 0
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=300) as response:
        while chunk := response.read(1 << 20):
            digest.update(chunk)
            size += len(chunk)
    return digest.hexdigest(), size


def version_key(version: str) -> tuple[int, ...]:
    return tuple(int(part) for part in re.findall(r"\d+", version))


def entry(tool_id, tool, version, platform, urls, archive, source, published=None):
    sha256, size = measure(urls[0])
    if published and published.lower() != sha256:
        raise RuntimeError(f"{urls[0]}: SHA-256 {sha256}, the source publishes {published}")
    print(f"  {tool_id:14} {platform:15} {version:9} {size // 1024:>7} KiB  {'checked' if published else 'recorded'}",
          file=sys.stderr)
    return {"id": tool_id, "tool": tool, "version": version, "platform": platform, "urls": urls,
            "sha256": sha256, "size": size, "archive": archive, "source": source,
            "hashFrom": "source" if published else "recorded"}


def windows_php() -> list[dict]:
    base = "https://windows.php.net/downloads/releases/"
    releases = json.loads(fetch(base + "releases.json"))
    minors = sorted((key for key in releases if re.fullmatch(r"\d+\.\d+", key)), key=version_key, reverse=True)
    out = []
    for minor in minors[:PHP_MINORS]:
        release = releases[minor]
        builds = [key for key in release if re.fullmatch(r"nts-vs\d+-x64", key)]
        if not builds:
            raise RuntimeError(f"windows.php.net {minor}: no NTS x64 build in {sorted(release)}")
        zip_info = release[sorted(builds)[-1]]["zip"]
        name = zip_info["path"]
        out.append(entry(f"php-{minor}", "php", release["version"], "windows-x86_64",
                         [base + name, base + "archives/" + name], "zip", "windows.php.net", zip_info.get("sha256")))
    return out


def static_php(minors: list[str]) -> list[dict]:
    base = "https://dl.static-php.dev/static-php-cli/common/"
    listing = fetch(base).decode("utf-8", "replace")
    found: dict[tuple[str, str], str] = {}
    for version, system, arch in re.findall(r"php-(\d+\.\d+\.\d+)-cli-(macos|linux)-(aarch64|x86_64)\.tar\.gz", listing):
        minor = ".".join(version.split(".")[:2])
        key = (minor, STATIC_PLATFORMS[(system, arch)])
        if minor in minors and (key not in found or version_key(version) > version_key(found[key])):
            found[key] = version
    out = []
    for minor in minors:
        for (system, arch), platform in STATIC_PLATFORMS.items():
            version = found.get((minor, platform))
            if not version:
                raise RuntimeError(f"static-php.dev: no PHP {minor} build for {platform}")
            name = f"php-{version}-cli-{system}-{arch}.tar.gz"
            out.append(entry(f"php-{minor}", "php", version, platform, [base + name], "tar.gz", "static-php.dev"))
    return out


def composer() -> list[dict]:
    versions = json.loads(fetch("https://getcomposer.org/versions"))
    stable = versions["stable"][0]
    url = "https://getcomposer.org" + stable["path"]
    published = fetch(url + ".sha256sum").decode().split()[0]
    return [entry("composer", "composer", stable["version"], "any", [url], "phar", "getcomposer.org", published)]


def mariadb() -> list[dict]:
    base = "https://archive.mariadb.org/"
    listing = fetch(base).decode("utf-8", "replace")
    patches = sorted(set(re.findall(rf"mariadb-({re.escape(MARIADB_SERIES)}\.\d+)/", listing)), key=version_key)
    if not patches:
        raise RuntimeError(f"archive.mariadb.org: no {MARIADB_SERIES} release")
    version = patches[-1]
    folder = f"{base}mariadb-{version}/winx64-packages/"
    sums = fetch(folder + "sha256sums.txt").decode()
    name = f"mariadb-{version}-winx64.zip"
    published = next((line.split()[0] for line in sums.splitlines() if line.strip().endswith(name)), None)
    if not published:
        raise RuntimeError(f"{folder}sha256sums.txt has no {name}")
    return [entry(f"mariadb-{MARIADB_SERIES}", "mariadb", version, "windows-x86_64", [folder + name], "zip",
                  "mariadb.org", published)]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    tools: list[dict] = []
    failed = False
    windows = []
    for name, step in [("PHP for Windows", windows_php), ("Composer", composer), ("MariaDB", mariadb)]:
        try:
            found = step()
            tools += found
            if step is windows_php:
                windows = found
        except Exception as error:  # noqa: BLE001 - each source on its own
            failed = True
            notice("error", f"{name}: {error}")
    minors = sorted({tool["id"].removeprefix("php-") for tool in windows}, key=version_key, reverse=True)
    try:
        tools += static_php(minors)
    except Exception as error:  # noqa: BLE001
        failed = True
        notice("error", f"PHP for macOS and Linux: {error}")

    order = {"php": 0, "composer": 1, "mariadb": 2}
    tools.sort(key=lambda t: (order[t["tool"]], [-n for n in version_key(t["version"])], t["platform"]))
    manifest = {"v": 1, "generated": time.strftime("%Y-%m-%d", time.gmtime()), "tools": tools}
    text = json.dumps(manifest, indent=2) + "\n"
    print(text)

    # The entries as annotations, a few per line (logs can't always be read)
    for start in range(0, len(tools), 3):
        notice("notice", "tools.json entries:\n" + "\n".join(json.dumps(t) for t in tools[start:start + 3]))

    if args.write and not failed:
        MANIFEST.write_text(text, encoding="utf-8")
    if args.check:
        committed = json.loads(MANIFEST.read_text(encoding="utf-8"))
        def strip(m): return [{k: v for k, v in t.items()} for t in m["tools"]]
        if strip(committed) != strip(manifest):
            notice("error", "libs/core/resources/tools.json differs from what the sources give now; "
                            "run tools/tool_manifest.py --write and commit it (the new entries are above)")
            return 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
