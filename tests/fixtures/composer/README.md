Real Composer and `php flo` output for the Packages tab (stage 8), from a
FloCMS project made from Packagist on GitHub's ubuntu-24.04 runner (PHP 8.4)
on 24 Oct 2026, by the one-off `capture/composer-packages` workflow:

- `outdated-all`: `composer outdated --direct --all --format=json` right after
  `composer create-project hostkurd/flocms` (flocms-core 2.8.0, 2.9.0 out)
- `outdated`: the same without `--all`
- `outdated-older*`: after `composer require hostkurd/flocms-uploader:1.2.0 -W`
  (`downgraded.txt`)
- `update`: `composer require "hostkurd/flocms-uploader:^1.2.1" -W`
- `show-api`, `show-admin`: `composer show <package> --all --format=json`
- `add-api` + `api-install`, `add-admin` + `admin-install`: adding a package
  as Studio does; `migrate`: then `php flo migrate --force` (no database
  set in that project: the real error)
- `outdated-after`: `composer outdated --direct --all --format=json` at the end

Each command has `.out` (stdout), `.err` (stderr) and `.code` (exit code).
