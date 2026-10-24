"""Make the stage 7 demos (app/demo/generate*.json) from the real output
of FloCMS's generators in tests/fixtures/generate (captured in a preview
copy of a real project), on top of the Database demo's project.

Usage: python3 tools/demo_generate.py
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tests" / "fixtures" / "generate"
DEMO = ROOT / "app" / "demo"


def text(name: str) -> str:
    return (FIXTURES / name).read_text(encoding="utf-8")


def lines(name: str) -> list:
    return [line for line in text(name + ".out").splitlines() + text(name + ".err").splitlines()]


def created_path(out: str, part: str) -> str:
    """The file name the generator printed (it has the time in it)."""
    return next(m.group(1) for m in re.finditer(r"Created (\S+)", out) if part in m.group(1))


def demo(name: str, generate: dict) -> None:
    base = json.loads((DEMO / "database.json").read_text(encoding="utf-8"))
    base["tab"] = "generate"
    base["generate"] = generate
    (DEMO / name).write_text(json.dumps(base, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


out = text("admin-resource.out")
demo("generate.json", {
    "generator": "admin-resource",
    "form": {"name": "Property", "table": "properties", "slug": "listings", "migration": True},
    "preview": {
        "state": "done",
        "shown": "php flo make:admin-resource Property --table=properties --slug=listings --migration",
        "lines": lines("admin-resource"),
        "files": [
            {"path": created_path(out, "Resource"), "kind": "added", "after": text("PropertyResource.php")},
            {"path": created_path(out, "migrations"), "kind": "added", "after": text("create_properties_table.php")},
        ],
    },
})

demo("generate-widget.json", {
    "generator": "widget",
    "form": {"name": "PostStats", "resource": "Post"},
    "preview": {
        "state": "done",
        "shown": "php flo make:admin-widget PostStats --resource=Post",
        "selected": 1,
        "lines": lines("admin-widget"),
        "files": [
            {"path": "admin/Widgets/PostStats.php", "kind": "added", "after": text("PostStats.php")},
            {"path": "config/admin.php", "kind": "changed", "before": text("admin.before.php"), "after": text("admin.after.php")},
        ],
    },
})

# The generator said no: the file is there
demo("generate-failed.json", {
    "generator": "admin-resource",
    "form": {"name": "Property"},
    "preview": {
        "state": "failed",
        "shown": "php flo make:admin-resource Property",
        "lines": lines("admin-resource-exists"),
        "error": "step_exit",
        "detail": text("admin-resource-exists.code").strip(),
        "message": next(l.strip()[len("[ERROR] "):] for l in lines("admin-resource-exists") if "[ERROR]" in l),
    },
})
