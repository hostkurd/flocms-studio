# Doctor

The **Doctor** tab shows what FloCMS's own health checks say about a project: `php flo doctor`, and for projects with the admin panel `php flo admin:doctor`. Studio runs them when you open a project and shows the result.

## When the checks run

Studio runs the checks:

- when you open a project;
- when you click **Run again**;
- after a generator in Studio wrote files, and after a package update.

The commands:

```bash
php flo doctor --format=json
php flo admin:doctor
```

The **Overview**'s **Doctor** card shows the summary (**All checks pass**, for example) and **Last checked**. **All checks** opens the tab.

## Framework checks

The summary at the top counts problems, warnings and checks that are OK. Under **Framework checks**, the checks that aren't OK come first. **12 more checks, all OK** (with the number) shows the rest; **Show fewer** hides them again.

The checks come from `php flo doctor`:

- **PHP version** and each **Extension**
- **Composer packages installed**
- the folders that must be writable
- **File `.env`** and **Keys in `.env`**
- `APP_KEY`, `APP_URL` and **Debug mode (`APP_DEBUG`)**
- **HTTPS**
- **Database** and **Migrations**
- **Rewrite rules (`public/.htaccess`)** and **Document root**
- **Maintenance mode**

Studio translates the check names. The details are FloCMS's own words.

A missing PHP extension shows here. Choose another PHP in [Settings > PHP and tools](settings.md#php-and-tools), or download one: the PHP Studio downloads for Windows comes with a `php.ini` that turns on the extensions `php flo doctor` checks.

## Admin panel checks

For a project with the admin panel, **Admin panel checks** shows what `php flo admin:doctor` says, section by section, with its summary. **Show the full output** shows the command's whole output.

See the framework's [installation check](/docs/2.0/installation#check-your-installation), the [command list](/docs/2.0/cli#application), and the admin panel's [commands](/docs/admin/0.39/overview#commands).
