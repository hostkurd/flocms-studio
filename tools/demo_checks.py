"""Fill the "checks" of the stage 5 demos (app/demo/*.json) from the real
captured output in tests/fixtures/flo, so the Doctor, Database and
Overview screenshots show what `php flo` really prints. The project path
and database name are changed to the demo project's.

Usage: python3 tools/demo_checks.py
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tests" / "fixtures" / "flo"


def fixture(name: str) -> str:
    text = (FIXTURES / name).read_text(encoding="utf-8") if (FIXTURES / name).exists() else ""
    return text.replace("/home/claude/work/full", "C:\\Sites\\fectumhellas").replace("studio_full", "fectum")


def run(name: str) -> dict:
    code = fixture(name + ".code").strip()
    return {"out": fixture(name + ".out"), "err": fixture(name + ".err"), "code": int(code or 0)}


def status_with_pending() -> dict:
    """Board 5: most migrations ran, one changed since, two wait."""
    lines = fixture("migrate-status-changed.out").splitlines()
    table = [line for line in lines if line.startswith("|")]
    border = next(line for line in lines if line.startswith("+"))
    width = [len(cell) for cell in border.split("+")[1:-1]]

    def row(*cells):
        return "|" + "|".join(" " + cell.ljust(w - 1) for cell, w in zip(cells, width)) + "|"

    rows = table[1:]
    rows += [row("2026_10_24_140500_add_featured_to_posts", "app", "", "Pending"),
             row("2026_10_24_141000_create_listings_table", "app", "", "Pending")]
    text = "\n".join([border, table[0], border, *rows, border, '2 pending; run "php flo migrate".']) + "\n"
    return {"out": text, "err": "", "code": 0}


def doctor_two_pending() -> dict:
    """The doctor run of the same state: two migrations wait."""
    result = run("doctor-pending.json")
    result["out"] = result["out"].replace("13 pending", "2 pending")
    return result


def checks(minutes: int, output: dict | None = None) -> dict:
    result = {
        "doctor": doctor_two_pending(),
        "adminDoctor": run("admin-doctor-problem"),
        "show": run("db-show.json"),
        "status": status_with_pending(),
        "adminList": run("admin-list"),
        "checkedMinutesAgo": minutes,
    }
    if output:
        result["output"] = output
    return result


def main() -> None:
    project = json.loads((ROOT / "app/demo/project.json").read_text(encoding="utf-8"))
    project["checks"] = checks(2, {
        "command": "optimize", "state": "done", "exitCode": 0,
        "shown": "php flo optimize --no-interaction --no-ansi",
        "lines": fixture("optimize.out").splitlines(),
    })
    (ROOT / "app/demo/project.json").write_text(json.dumps(project, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    database = dict(project)
    database["tab"] = "database"
    database["checks"] = checks(2, {
        "command": "migrate", "state": "done", "exitCode": 0,
        "shown": "php flo migrate --no-interaction --no-ansi",
        "lines": fixture("migrate.out").splitlines()[:6],
    })
    (ROOT / "app/demo/database.json").write_text(json.dumps(database, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    doctor = dict(project)
    doctor["tab"] = "doctor"
    doctor["checks"] = checks(2)
    (ROOT / "app/demo/doctor.json").write_text(json.dumps(doctor, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    logs = json.loads((ROOT / "app/demo/logs.json").read_text(encoding="utf-8"))
    logs["checks"] = checks(2)
    (ROOT / "app/demo/logs.json").write_text(json.dumps(logs, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
