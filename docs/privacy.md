# Privacy and code signing

FloCMS Studio doesn't collect or send any information about you, your computer or your projects. It has no analytics, no crash reports and no update check. This page says when Studio connects to the internet, and how its Windows files are signed.

## What Studio connects to

Studio connects to the internet only for these:

- **Downloading a tool** you chose in Studio, from its official site:
  - PHP from [windows.php.net](https://windows.php.net) on Windows, or from [static-php-cli](https://static-php.dev) on macOS and Linux
  - Composer from [getcomposer.org](https://getcomposer.org)
  - MariaDB from [archive.mariadb.org](https://archive.mariadb.org)

  These sites see an ordinary download request.
- **Composer**, which Studio runs in your project when you create a project, when you add or update packages, and when you open a project's **Packages** tab (`composer outdated`, to show which packages have updates). Composer talks to [Packagist](https://packagist.org) and GitHub as it always does. It sends the names of your project's packages, not your code.

## What stays on your computer

Everything else:

- Studio's settings are in your user's settings folder.
- The passwords Studio saves are in the system's secure store.
- The database servers and websites Studio starts listen on `127.0.0.1`, unless you choose otherwise ([Settings > Server](settings.md#server)).

Uninstalling Studio leaves your settings and projects in place. Delete them yourself if you want them gone.

## How Studio checks downloads

Studio downloads only the files in the tool list built into it. Each entry has the file's exact SHA-256 and size; Studio never takes an address from a web page or a file. The list is made by the project's CI, which checks each hash against the one the source publishes, where it publishes one. Studio checks every file before it uses it, and deletes a file that doesn't match.

## Code signing

Free code signing provided by [SignPath.io](https://about.signpath.io), certificate by [SignPath Foundation](https://signpath.org).

Studio's Windows release files are signed through SignPath Foundation's program for open source projects. Windows shows the publisher as **SignPath Foundation**. The macOS and Linux builds aren't signed by this program.

What is signed:

- `Studio.exe`, in the portable folder and in the installer. It is one program: Studio's code with Qt linked in, built from Qt's official source code.
- The installer, `FloCMS-Studio-<version>-x64.msi`.

The license texts that ship with Studio (`LICENSE.txt`, `THIRD-PARTY-NOTICES.txt`) aren't signed.

How signing works:

- Only files built from the repository's source code, by its GitHub Actions release workflow, from a version tag, are signed. Nothing is signed by hand, and nothing built outside GitHub Actions is signed.
- Each signed file says it is FloCMS Studio and carries the release's version (**Properties > Details**). SignPath checks both before signing.
- An approver approves each signing request, one by one, after checking that it comes from the release workflow, for a version tag on `main`.
- Every team member uses multi-factor authentication on GitHub and on SignPath.

Releases made before SignPath accepted the project are unsigned and say so in their release notes.

The full policy, with the team's roles, is in [CODE_SIGNING.md](../CODE_SIGNING.md).

## Report a problem

If you think a file signed for FloCMS Studio is not what this policy describes, [open an issue](https://github.com/hostkurd/flocms-studio/issues) or write to SignPath Foundation at [support@signpath.org](mailto:support@signpath.org).
