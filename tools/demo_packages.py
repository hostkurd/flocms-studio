"""Make the stage 8 demos (app/demo/packages*.json) from the real Composer
output in tests/fixtures/composer (a project made from Packagist; see its
README.md), on top of the Database demo's project.

Usage: python3 tools/demo_packages.py
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tests" / "fixtures" / "composer"
DEMO = ROOT / "app" / "demo"


def text(name: str) -> str:
    return (FIXTURES / name).read_text(encoding="utf-8")


def run(name: str) -> dict:
    return {"out": text(name + ".out"), "err": text(name + ".err"), "code": int(text(name + ".code").strip() or 0)}


def installed(outdated: str) -> list:
    """composer.lock's hostkurd packages, as Composer reported them."""
    packages = json.loads(text(outdated + ".out"))["installed"]
    return [{"name": p["name"], "version": p["version"].lstrip("v")} for p in packages if p["name"].startswith("hostkurd/")]


def demo(name: str, outdated: str, packages: dict, dialog: str = "") -> None:
    base = json.loads((DEMO / "database.json").read_text(encoding="utf-8"))
    base["tab"] = "packages"
    if dialog:
        base["dialog"] = dialog
    # The project Composer made: a website, no admin panel, no API yet
    base["current"]["type"] = "site"
    base["current"]["packages"] = installed(outdated)
    base["current"]["otherPackages"] = 0
    base["current"]["others"] = []
    for project in base["projects"]:
        if project["name"] == base["current"]["name"]:
            project["hasAdmin"] = False
    base.pop("checks", None)
    base["packages"] = packages
    (DEMO / name).write_text(json.dumps(base, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


# Board 8: an update to review
demo("packages.json", "outdated-all", {"outdated": run("outdated-all"), "checkedMinutesAgo": 2}, "hostkurd/flocms-core")

# The update running: Composer's real output, then migrate
lines = ["› composer require hostkurd/flocms-uploader:^1.2.1 -W"] + text("update.err").splitlines() + ["› php flo migrate --force"]
demo("packages-update.json", "outdated-older-all", {
    "outdated": run("outdated-older-all"),
    "checkedMinutesAgo": 5,
    "run": {
        "kind": "update", "name": "hostkurd/flocms-uploader", "from": "1.2.0", "to": "1.2.1", "state": "running",
        "steps": [{"shown": "composer require hostkurd/flocms-uploader:^1.2.1 -W", "state": "done"},
                  {"shown": "php flo migrate --force", "state": "running"}],
        "lines": lines,
    },
})

# Adding the API
demo("packages-add.json", "outdated-all", {"outdated": run("outdated-all"), "checkedMinutesAgo": 2}, "hostkurd/flocms-api")
