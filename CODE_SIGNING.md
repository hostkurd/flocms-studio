# Code signing policy

Free code signing provided by [SignPath.io](https://about.signpath.io), certificate by [SignPath Foundation](https://signpath.org).

FloCMS Studio's Windows release files are signed through SignPath Foundation's program for open source projects. Windows shows the publisher as **SignPath Foundation**. The macOS and Linux builds aren't signed by this program.

## What is signed

Only files built from this repository's source code, by this repository's GitHub Actions workflow ([`.github/workflows/release.yml`](.github/workflows/release.yml)), from a version tag:

- `Studio.exe`, `FloStudioCore.dll`, `FloStudioDatabase.dll` and `FloStudioSecurity.dll`, in the portable folder and in the installer
- the installer, `FloCMS-Studio-<version>-x64.msi`, built with the [WiX Toolset](https://wixtoolset.org)

The other files that ship with Studio (Qt's libraries, plugins and QML files, and Microsoft's Visual C++ runtime) are other projects' work. They are shipped as their makers built them, and never signed with this certificate. The release workflow checks this: it fails if signing changed any file other than ours.

Every signed file says it is FloCMS Studio and carries the release's version (Properties > Details), and SignPath checks both before signing. What SignPath signs is set in [`.signpath/artifact-configurations/`](.signpath/artifact-configurations/).

Nothing is signed by hand, and nothing built outside GitHub Actions is signed.

## Team roles

| Role | Members |
|---|---|
| Committers and reviewers | [khalat-jalal](https://github.com/khalat-jalal), [sarmad-qadir](https://github.com/sarmad-qadir) |
| Approvers | [khalat-jalal](https://github.com/khalat-jalal), [sarmad-qadir](https://github.com/sarmad-qadir) |

- **Committers and reviewers** may change the source code. Every change goes to `main` through a pull request that the other member reviews.
- **Approvers** approve each signing request in SignPath, one by one, after checking that it comes from the release workflow, for a version tag on `main`.

Every member uses multi-factor authentication on GitHub and on SignPath.

## Privacy

FloCMS Studio doesn't collect or send any information about you, your computer or your projects. It has no analytics, no crash reports and no update check.

Studio connects to the internet only for these:

- **Downloading a tool** you chose in Studio, from its official site: PHP from [windows.php.net](https://windows.php.net) (Windows) or [static-php-cli](https://static-php.dev) (macOS, Linux), Composer from [getcomposer.org](https://getcomposer.org), MariaDB from [archive.mariadb.org](https://archive.mariadb.org). These sites see an ordinary download request.
- **Composer**, which Studio runs in your project when you create a project or add or update packages, and when you open a project's Packages tab (`composer outdated`, to show which packages have updates). Composer talks to [Packagist](https://packagist.org) and GitHub as it always does; it sends the names of your project's packages, not your code.

Everything else stays on your computer: Studio's settings are in your user's settings folder, and the database servers and websites it starts listen on `127.0.0.1` unless you choose otherwise. Uninstalling Studio leaves your settings and projects in place; delete them yourself if you want them gone.

## Reporting a problem

If you think a file signed for FloCMS Studio is not what this policy describes, please [open an issue](https://github.com/hostkurd/flocms-studio/issues) or write to SignPath Foundation at [support@signpath.org](mailto:support@signpath.org).

## Setting up (for the maintainers)

Until SignPath Foundation accepts the project, the release workflow makes unsigned draft releases. Once it does:

1. In SignPath, create the project with the trusted build system **GitHub.com**, a release signing policy that needs an approver, and the two artifact configurations from `.signpath/artifact-configurations/` named `studio-files` and `studio-installer`.
2. In this repository's settings (Secrets and variables > Actions), add the variables `SIGNPATH_ORGANIZATION_ID`, `SIGNPATH_PROJECT_SLUG` and `SIGNPATH_SIGNING_POLICY_SLUG`, and the secret `SIGNPATH_API_TOKEN` (a SignPath CI user with submitter rights only).
3. Tag a release (`git tag v0.1.0 && git push origin v0.1.0`). The workflow sends two signing requests, one for Studio's files and one for the installer; approve each in SignPath, then read the draft release on GitHub and publish it.
