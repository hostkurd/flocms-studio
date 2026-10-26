"""Build a static Qt from the official source archives, for the final single-file release (D6).

Used by CI (ci.yml, static-qt job) for every release build.

Qt does not publish static builds, so CI builds one and caches it. Only the
modules Studio needs are built: qtbase, qtshadertools, qtsvg, qtdeclarative
and the Linguist tools from qttools.

  Windows: MSVC, static C++ runtime (/MT), so Studio.exe needs no DLLs.
  macOS:   universal (x86_64 + arm64), Qt's own minimum macOS version.

Each archive is checked against Qt's md5sums.txt before use. A finished
module leaves a marker in <prefix>/share/flostudio/, and a later run skips
it, so CI can resume a build that ran out of time from a partial cache. The license
texts and the third-party attributions of the built modules are copied into
<prefix>/share/flostudio/ so the release can ship them (LGPL notices).

Usage:
  python build_static_qt.py --version 6.12.0 --prefix <dir> --work <dir> [--universal]

Progress and timings are printed as GitHub annotations, and a failing step
prints the end of its log as errors: job logs can't always be read later.
"""
import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.request
from pathlib import Path

MODULES = [
    # (module, extra configure arguments)
    ("qtbase", []),
    ("qtshadertools", []),
    ("qtsvg", []),
    ("qtdeclarative", []),
    # Studio needs lupdate/lrelease; the two big GUI tools are skipped.
    ("qttools", ["-no-feature-assistant", "-no-feature-designer"]),
]
SKIP_DIRS = {"tests", "examples"}  # not built (-nomake), and deep paths on Windows
WINDOWS = platform.system() == "Windows"
GITHUB = bool(os.environ.get("GITHUB_ACTIONS"))


def notice(message: str) -> None:
    print(f"::notice::{message}" if GITHUB else message, flush=True)


def fail(message: str, log: Path | None = None) -> None:
    if log and log.exists():
        lines = log.read_text(errors="replace").splitlines()
        print("\n".join(lines[-60:]))
        # The cause is usually a "CMake Error" or "ERROR:" block well before
        # the end; report it with the lines after it, then the end of the log.
        picked = []
        for i, line in enumerate(lines):
            if "CMake Error" in line or line.startswith("ERROR") or ": error" in line or "error:" in line:
                picked += [l for l in lines[i:i + 6] if l.strip()]
            if len(picked) >= 30:
                break
        for line in picked[:30] + lines[-10:]:
            print(f"::error::{line}" if GITHUB else line)
    sys.exit(f"::error::{message}" if GITHUB else message)


def fetch(url: str) -> bytes:
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url, timeout=300) as response:
                return response.read()
        except OSError as error:
            if attempt == 2:
                fail(f"download failed: {url}: {error}")
            time.sleep(10)
    raise AssertionError


def run(args: list[str], cwd: Path, log: Path) -> None:
    with log.open("a", encoding="utf-8", errors="replace") as out:
        out.write(f"\n$ {' '.join(args)}\n")
        out.flush()
        code = subprocess.run(args, cwd=cwd, stdout=out, stderr=subprocess.STDOUT).returncode
    if code != 0:
        fail(f"{' '.join(args[:3])}... failed with exit code {code} (log {log.name})", log)


def extract(archive: Path, target: Path) -> Path:
    with tarfile.open(archive) as tar:
        members = []
        for member in tar.getmembers():
            parts = Path(member.name).parts
            if len(parts) > 1 and parts[1] in SKIP_DIRS:
                continue
            members.append(member)
        tar.extractall(target, members=members, filter="tar")
        top = Path(tar.getmembers()[0].name).parts[0]
    return target / top


def disk_free(path: Path) -> str:
    return f"{shutil.disk_usage(path).free / 2**30:.1f} GB free"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--universal", action="store_true")
    args = parser.parse_args()

    version = args.version
    minor = ".".join(version.split(".")[:2])
    base = f"https://download.qt.io/official_releases/qt/{minor}/{version}/submodules"
    prefix = args.prefix.resolve()
    work = args.work.resolve()
    logs = work / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    share = prefix / "share" / "flostudio"

    sums = {}
    for line in fetch(f"{base}/md5sums.txt").decode().splitlines():
        parts = line.split()
        if len(parts) == 2:
            sums[parts[1].lstrip("*")] = parts[0]
    if not sums:
        fail(f"no checksums in {base}/md5sums.txt")

    share.mkdir(parents=True, exist_ok=True)
    attributions_file = share / "qt-attributions.json"
    attributions = json.loads(attributions_file.read_text(encoding="utf-8")) if attributions_file.exists() else []
    started = time.time()
    for module, extra in MODULES:
        marker = share / f"built-{module}"
        if marker.exists():
            notice(f"{module}: already built (from the cache), skipped")
            continue
        t0 = time.time()
        name = f"{module}-everywhere-src-{version}.tar.xz"
        if name not in sums:
            fail(f"{name} is not listed in md5sums.txt")
        data = fetch(f"{base}/{name}")
        if hashlib.md5(data).hexdigest() != sums[name]:
            fail(f"checksum mismatch for {name}")
        archive = work / name
        archive.write_bytes(data)
        del data
        source = extract(archive, work / "src")
        archive.unlink()

        module_attributions = []
        for path in source.rglob("qt_attribution.json"):
            try:
                content = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                continue
            for entry in content if isinstance(content, list) else [content]:
                entry["QtModule"] = module
                module_attributions.append(entry)
        if module == "qtbase":
            (share / "licenses").mkdir(parents=True, exist_ok=True)
            for text in (source / "LICENSES").glob("*.txt"):
                shutil.copy(text, share / "licenses" / text.name)

        build = work / "build" / module
        build.mkdir(parents=True, exist_ok=True)
        log = logs / f"{module}.log"
        if module == "qtbase":
            configure = [str(source / ("configure.bat" if WINDOWS else "configure")),
                         "-static", "-release", "-opensource", "-confirm-license",
                         "-prefix", str(prefix), "-nomake", "examples", "-nomake", "tests"]
            if WINDOWS:
                configure.append("-static-runtime")
            if args.universal:
                configure += ["--", "-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64"]
        else:
            tool = prefix / "bin" / ("qt-configure-module.bat" if WINDOWS else "qt-configure-module")
            configure = [str(tool), str(source)] + extra
        if WINDOWS:
            configure = ["cmd", "/c"] + configure
        run(configure, build, log)
        run(["cmake", "--build", ".", "--parallel", str(os.cpu_count() or 2)], build, log)
        run(["cmake", "--install", "."], build, log)

        shutil.rmtree(build, ignore_errors=True)
        shutil.rmtree(source, ignore_errors=True)
        attributions = [a for a in attributions if a.get("QtModule") != module] + module_attributions
        attributions_file.write_text(json.dumps(attributions, indent=1), encoding="utf-8")
        marker.write_text(time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), encoding="utf-8")
        notice(f"{module}: built in {(time.time() - t0) / 60:.0f} min ({disk_free(work)})")

    (share / "qt-build.json").write_text(json.dumps({
        "version": version,
        "modules": [m for m, _ in MODULES],
        "static_runtime": WINDOWS,
        "universal": args.universal,
        "source": base,
    }, indent=1), encoding="utf-8")
    notice(f"static Qt {version} ready in {(time.time() - started) / 60:.0f} min at {prefix}")


if __name__ == "__main__":
    main()
