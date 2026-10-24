#!/usr/bin/env bash
# Makes a real FloCMS project for tst_realproject (tests/tst_realproject.cpp)
# with Composer from Packagist, as a Studio user would:
#   .github/scripts/real_project.sh <folder> sqlite        a website, SQLite
#   .github/scripts/real_project.sh <folder> mysql         website + admin panel, MySQL
#                                                          (root/root on 127.0.0.1)
set -euo pipefail
site="$1"; database="$2"
fail() { echo "::error::real project: $*"; exit 1; }

composer create-project hostkurd/flocms "$site" --no-interaction --no-progress > create.log 2>&1 \
  || { tail -20 create.log; fail "composer create-project failed: $(tail -1 create.log)"; }
cd "$site"
[ -f .env ] || cp .env.example .env

# .env as the new project wizard will write it (python: the same on every runner)
python - "$database" <<'PY'
import re, sys
database = sys.argv[1]
values = ({"DB_TYPE": "sqlite", "DB_NAME": "storage/database.sqlite"} if database == "sqlite" else
          {"DB_TYPE": "mysql", "DB_HOST": "127.0.0.1", "DB_PORT": "3306", "DB_NAME": "studio_ci",
           "DB_USERNAME": "root", "DB_PASSWORD": "root"})
text = open(".env", encoding="utf-8").read()
for key, value in values.items():
    text, count = re.subn(rf"(?m)^{key}=.*$", f"{key}={value}", text)
    if count == 0:
        text += f"{key}={value}\n"
open(".env", "w", encoding="utf-8").write(text)
PY

if [ "$database" = "mysql" ]; then
  php flo setup --as=site-admin --no-interaction --no-ansi > setup.log 2>&1 \
    || { tail -20 setup.log; fail "php flo setup --as=site-admin failed: $(tail -1 setup.log)"; }
fi
php flo migrate --force --no-interaction --no-ansi > migrate.log 2>&1 \
  || { tail -20 migrate.log; fail "php flo migrate failed: $(tail -1 migrate.log)"; }
versions=$(php -r '$l=json_decode(file_get_contents("composer.lock"),true); foreach($l["packages"] as $p) if (str_starts_with($p["name"],"hostkurd/")) echo $p["name"]," ",$p["version"],"; ";')
echo "::notice::real project ($database): $versions"
