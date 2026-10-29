# Studio and the command line

Every action in Studio runs a real `php flo` or `composer` command in the project, and Studio shows the command. Nothing Studio does needs Studio: you can type the same commands in a terminal, and a project made in Studio works without it.

## How Studio runs commands

- `php flo` commands run with the project's own `flo` launcher (or `vendor/bin/flo`), in the project folder.
- Studio adds `--no-interaction` and `--no-ansi` to every command, so none waits for an answer. Composer's `create-project`, `require`, `install` and `update` also get `--no-progress`. The commands on screen leave these options out.
- The project's `.env` decides what the commands see: Studio removes the environment variables `.env` sets from the command's environment.
- Studio runs only the commands in its own list. It never runs a Composer command on another folder (`--working-dir`).

## What Studio runs

| What you do in Studio | The command it runs | Framework docs |
|---|---|---|
| **Create the project** (new project) | `composer create-project hostkurd/flocms <name>` | [Installation](/docs/2.0/installation#create-a-project) |
| What you're building (new project) | `php flo setup --as=site-admin` (or `admin`, `api-admin`, `api`) | [Choose what you build](/docs/2.0/setups#the-five-setups) |
| **Also add the JSON API** (new project) | `composer require hostkurd/flocms-api`, then `php flo api:install` | [Add an API later](/docs/2.0/setups#add-an-api-later) |
| Check the database (new project, MySQL) | `php flo db:check` | [Checking the connection](/docs/2.0/database#checking-the-connection) |
| First admin (new project) | `php flo user:create --name=… --email=… --role=super-admin --password-stdin` | [Creating users](/docs/2.0/authentication#creating-users) |
| **Start server** | `php flo serve --port=8000 --host=127.0.0.1` | [Start a local server](/docs/2.0/installation#start-a-local-server) |
| **Clear cache** | `php flo cache:clear` | [Cache](/docs/2.0/cli#cache) |
| **Optimize** | `php flo optimize` | [Cache](/docs/2.0/cli#cache) |
| **Doctor** tab | `php flo doctor --format=json` | [Check your installation](/docs/2.0/installation#check-your-installation) |
| **Admin panel checks** | `php flo admin:doctor` | [Admin commands](/docs/admin/0.39/overview#commands) |
| **Database** tab: connection and tables | `php flo db:show --format=json` | [Database commands](/docs/2.0/cli#database) |
| **Database** tab: migrations | `php flo migrate:status` | [Running migrations](/docs/2.0/migrations#running-migrations) |
| **Open in admin** links | `php flo admin:list` | [Admin commands](/docs/admin/0.39/overview#commands) |
| **Run migrations** | `php flo migrate` (production: `--force`) | [Running migrations](/docs/2.0/migrations#running-migrations) |
| **Roll back last batch** | `php flo migrate:rollback` (production: `--force`) | [Running migrations](/docs/2.0/migrations#running-migrations) |
| **Seed…** | `php flo db:seed` (production: `--force`) | [Running seeders](/docs/2.0/seeding#running-seeders) |
| **Drop all and migrate…** | `php flo migrate:fresh --force` | [Database commands](/docs/2.0/cli#database) |
| **Generate**: admin resource | `php flo make:admin-resource <Name> …` | [Resources](/docs/admin/0.39/overview#resources) |
| **Generate**: admin page | `php flo make:admin-page <Name> …` | [Custom pages](/docs/admin/0.39/custom-pages) |
| **Generate**: block | `php flo make:admin-block <Name>` | [Builder](/docs/admin/0.39/builder) |
| **Generate**: widget | `php flo make:admin-widget <Name> …` | [Dashboard widgets](/docs/admin/0.39/widgets) |
| **Generate**: controller | `php flo make:controller <Name> …` | [Generators](/docs/2.0/generators) |
| **Generate**: migration | `php flo make:migration <name> …` | [Creating a migration](/docs/2.0/migrations#creating-a-migration) |
| **Packages** tab, **Check for updates** | `composer outdated --direct --all --format=json` | [Framework and packages](/docs/2.0/packages#versions-in-this-guide) |
| **Update** a package | `composer require "<package>:^<version>" -W`, then `php flo admin:install` or `php flo api:install` (when it is one of those), then `php flo migrate --force` | [Upgrade guide](/docs/2.0/upgrade) |
| **Add the admin panel** | `composer require hostkurd/flocms-admin -W`, `php flo admin:install`, `php flo migrate --force` | [Add the admin panel later](/docs/2.0/setups#add-the-admin-panel-later) |
| **Add the API** | `composer require hostkurd/flocms-api -W`, `php flo api:install`, `php flo migrate --force` | [Add an API later](/docs/2.0/setups#add-an-api-later) |

Two steps of a new project are not commands: Studio writes `.env` itself, and on its own MariaDB it makes the database and user with SQL. See [Your first project](first-project.md#step-4-create).

## Studio's own options

The Studio program takes a few options of its own:

```bash
flocms-studio --version     # the version and build details
flocms-studio --help        # every option
flocms-studio --open <folder>  # open this FloCMS project (FloServer uses this)
```

`--open` adds the folder to Studio if it isn't there yet and opens it. When Studio is already running, that Studio opens it and the new one closes. On Windows, Studio records where its program is in the registry (`HKEY_CURRENT_USER\Software\HostKurd\FloCMS Studio`, value `Path`), so FloServer can find it.

On Windows the program is `Studio.exe`; on macOS it is inside `FloCMS Studio.app`. The [About page](troubleshooting.md#report-a-problem) has **Copy version info** with the same details.
