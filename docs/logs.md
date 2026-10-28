# Logs

The **Logs** tab shows what a project prints, as one live list: the output of its server (`php flo serve`) and FloCMS's own log, `storage/logs/app.log`. Studio's own lines, such as the command it ran and when the server started or stopped, are in the list too.

## What you see

- **All**, **Server** or **App log** chooses which lines to show.
- **Problems only** shows only the problems, with their number.
- **Search** filters the lines by text.
- **Copy** copies the lines shown.
- **Clear** empties the list in Studio. The log file stays as it is.
- **Open log file** opens `storage/logs/app.log`. When there is no such file yet, Studio says so: FloCMS writes it at the first message.

When the list is empty, Studio says: "Nothing logged yet. Start the server to see what it prints; FloCMS's own log appears as it is written."

The newest lines come in at the end. While you look at the end, the list follows them; scroll up to read older lines.

## The server's lines

These are the lines PHP's built-in server prints. Studio leaves out the connection lines (accepted, closing). It marks:

- requests that answered 500 or higher, PHP errors and exceptions as problems;
- requests that answered 400 to 499, and PHP warnings, notices and deprecations, as warnings.

## The app log's lines

When you open a project, Studio shows the last lines already in `storage/logs/app.log`, then adds each new line as FloCMS writes it. It marks:

- `error`, `critical`, `alert` and `emergency` lines as problems;
- `warning` lines as warnings.

A "page not found" or another 4xx HTTP exception is not marked: it is not the site's problem.

Studio keeps the newest 10,000 lines of each project.

See the framework's [logging guide](/docs/2.0/errors#logging) for what FloCMS writes and the `log:` commands.
