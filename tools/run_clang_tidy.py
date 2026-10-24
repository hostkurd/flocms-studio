"""Run clang-tidy on FloStudio's own sources (D11).

Reads compile_commands.json from a build folder and checks every file under
app/, libs/ and tests/, skipping files Qt generates into the build folder.
Exits non-zero when clang-tidy reports anything.

Usage: python3 tools/run_clang_tidy.py build [clang-tidy binary]
"""
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OWN = ("app", "libs", "tests")


def main() -> int:
    build = Path(sys.argv[1]).resolve()
    tidy = sys.argv[2] if len(sys.argv) > 2 else "clang-tidy"
    entries = json.loads((build / "compile_commands.json").read_text())
    files = sorted({
        Path(e["file"]).resolve() for e in entries
        if not Path(e["file"]).resolve().is_relative_to(build)
        and Path(e["file"]).resolve().relative_to(ROOT).parts[0] in OWN
    })
    if not files:
        print("no source files found in compile_commands.json")
        return 1

    def check(path: Path) -> tuple[Path, int, str]:
        result = subprocess.run([tidy, "-p", str(build), "--quiet", str(path)],
                                capture_output=True, text=True)
        return path, result.returncode, (result.stdout + result.stderr).strip()

    failed = 0
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 2) as pool:
        for path, code, output in pool.map(check, files):
            name = path.relative_to(ROOT)
            # clang-tidy prints "N warnings generated" for Qt headers it then
            # filters out; only lines naming our files matter.
            findings = [line for line in output.splitlines() if ": error:" in line or ": warning:" in line]
            if code != 0 or findings:
                failed += 1
                print(f"FAIL  {name}")
                for line in findings:
                    print(f"  {line}")
                    if os.environ.get("GITHUB_ACTIONS"):
                        print(f"::error::{line}")
                if code != 0 and not findings:
                    print(output)
            else:
                print(f"ok    {name}")
    print(f"{len(files)} file(s), {failed} with findings")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
