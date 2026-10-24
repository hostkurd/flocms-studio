"""Capture real `php flo` output as test fixtures for FloStudioCore.

Studio reads the output of FloCMS's own commands. The tests check the
readers against output captured from a real project, so when FloCMS
changes a format the fixtures are refreshed with this script and the tests
show what changed.

The project must be installed (vendor/) with flocms-admin, and its .env
must point at a database that may be emptied: the script runs
`migrate:fresh`.

Each fixture is three files: NAME.out (stdout), NAME.err (stderr) and
NAME.code (the exit code), plus versions.txt with the package versions.

Linux and macOS (it stops the server by its process group).

Usage:
  python3 tools/capture_fixtures.py <project> [--out tests/fixtures/flo] [--php php]
"""
import argparse
import json
import os
import signal
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


class Capture:
    def __init__(self, project: Path, out: Path, php: str):
        self.project = project
        self.out = out
        self.php = php

    def run(self, name: str, *args: str, stdin: str | None = None, env: dict | None = None) -> int:
        command = [self.php, str(self.project / "flo"), *args, "--no-ansi", "--no-interaction"]
        result = subprocess.run(
            command, cwd=self.project, input=stdin, capture_output=True, text=True,
            env={**os.environ, **(env or {})}, timeout=300,
        )
        if name:
            (self.out / f"{name}.out").write_text(result.stdout, encoding="utf-8")
            (self.out / f"{name}.err").write_text(result.stderr, encoding="utf-8")
            (self.out / f"{name}.code").write_text(f"{result.returncode}\n", encoding="utf-8")
            print(f"{name:28} exit {result.returncode}")
        return result.returncode


def capture_server(c: Capture, project: Path, port: int = 8123) -> None:
    """`php flo serve` with a few requests, and what core wrote to app.log.

    Two pages are added to public/ for the run (a fatal error and a
    warning) and removed afterwards.
    """
    public = project / "public"
    pages = {
        "studio-fatal.php": "<?php\nundefined_studio_function();\n",
        "studio-warning.php": "<?php\ntrigger_error('Studio test warning', E_USER_WARNING);\necho 'ok';\n",
    }
    log = project / "storage" / "logs" / "app.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text("", encoding="utf-8")
    for name, text in pages.items():
        (public / name).write_text(text, encoding="utf-8")
    command = [c.php, str(project / "flo"), "serve", f"--port={port}", "--no-ansi", "--no-interaction"]
    server = subprocess.Popen(command, cwd=project, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                              start_new_session=True)  # php -S runs as its child
    try:
        base = f"http://127.0.0.1:{port}"
        for _ in range(50):
            try:
                urllib.request.urlopen(base + "/themes/default/css/style.css", timeout=1).close()
                break
            except OSError:
                time.sleep(0.1)
        for path in ["/", "/en", "/themes/default/css/style.css", "/en/no-such-page",
                     "/studio-fatal.php", "/studio-warning.php", "/no-such-file.css", "/en/admin"]:
            try:
                urllib.request.urlopen(base + path, timeout=10).close()
            except OSError:
                pass  # 404 and 500 are what is being captured
        time.sleep(0.5)
    finally:
        os.killpg(server.pid, signal.SIGTERM)
        out, err = server.communicate(timeout=10)
        for name in pages:
            (public / name).unlink(missing_ok=True)
    (c.out / "serve.out").write_text(out, encoding="utf-8")
    (c.out / "serve.err").write_text(err, encoding="utf-8")
    (c.out / "app.log").write_text(log.read_text(encoding="utf-8"), encoding="utf-8")
    print(f"{'serve':28} {len(err.splitlines())} server lines")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("project", type=Path)
    parser.add_argument("--out", type=Path, default=ROOT / "tests" / "fixtures" / "flo")
    parser.add_argument("--php", default="php")
    args = parser.parse_args()
    project = args.project.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    c = Capture(project, args.out, args.php)

    # An empty database: everything pending
    c.run("migrate-rollback-all", "migrate:fresh", "--force")
    c.run("", "migrate:rollback", "--force", "--step=1000")
    c.run("migrate-status-pending", "migrate:status")
    c.run("doctor-pending.json", "doctor", "--format=json")
    c.run("migrate", "migrate", "--force")

    # A migrated project
    c.run("migrate-status", "migrate:status")
    c.run("migrate-status-none", "migrate:status", "--package=nothing-here")
    c.run("doctor.json", "doctor", "--format=json")
    c.run("doctor", "doctor")
    c.run("about.json", "about", "--format=json")
    c.run("db-check", "db:check")
    c.run("db-show.json", "db:show", "--format=json")
    c.run("db-show", "db:show")
    c.run("admin-doctor", "admin:doctor")
    c.run("user-create", "user:create", "--name=Studio Admin", "--email=admin@example.com",
          "--role=super-admin", "--password-stdin", stdin="Studio-test-123\n")
    c.run("user-create-again", "user:create", "--name=Studio Admin", "--email=admin@example.com",
          "--role=super-admin", "--password-stdin", stdin="Studio-test-123\n")
    c.run("admin-list", "admin:list")
    c.run("unknown-command", "no-such-command")

    # A database Studio cannot reach: the real environment wins over .env
    # (core's Env::load never overrides it)
    wrong = {"DB_PASSWORD": "wrong-password"}
    c.run("migrate-status-denied", "migrate:status", env=wrong)
    c.run("db-show-denied.json", "db:show", "--format=json", env=wrong)
    c.run("doctor-denied.json", "doctor", "--format=json", env=wrong)

    # admin:doctor with a problem: the posts table rolled back
    c.run("", "migrate:rollback", "--step=1", "--force")
    c.run("admin-doctor-problem", "admin:doctor")
    c.run("", "migrate", "--force")

    # Migrations whose file changed after it ran, or is gone
    migrations = sorted((project / "database" / "migrations").glob("*.php"))
    changed, gone = migrations[-1], migrations[-2]
    original = changed.read_bytes()
    moved = gone.with_suffix(".studio-moved")
    try:
        changed.write_bytes(original + b"\n// changed by capture_fixtures.py\n")
        gone.rename(moved)
        c.run("migrate-status-changed", "migrate:status")
    finally:
        changed.write_bytes(original)
        if moved.exists():
            moved.rename(gone)

    capture_server(c, project)

    lock_file = project / "composer.lock"
    if (project / "composer.local.lock").is_file():  # a sandbox install without Packagist
        lock_file = project / "composer.local.lock"
    lock = json.loads(lock_file.read_text(encoding="utf-8"))
    versions = sorted(f"{p['name']} {p['version']}" for p in lock.get("packages", [])
                      if p["name"].startswith("hostkurd/"))
    php = subprocess.run([args.php, "-r", "echo PHP_VERSION;"], capture_output=True, text=True).stdout
    (args.out / "versions.txt").write_text(f"PHP {php}\n" + "\n".join(versions) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
