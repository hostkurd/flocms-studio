#pragma once

#include <flostudio/core/export.h>

#include <QString>

namespace FloStudio::Core {

// Reading the two logs Studio shows (stage 4): what `php flo serve` prints
// (PHP's built-in server, on stderr) and storage/logs/app.log (core's
// Logger). Each line is read on its own, as it arrives; a line nothing
// matches is Other and shown as it is.
//
// The built-in server prints a request line only for files it serves
// itself (CSS, images) and for errors: pages that go through index.php
// print none. tests/fixtures/flo/serve.err is a real run.

enum class ServerLineKind {
    Started,    // PHP 8.3.6 Development Server (http://127.0.0.1:8000) started
    Connection, // 127.0.0.1:51234 Accepted / Closing
    Request,    // 127.0.0.1:51234 [200]: GET /style.css (- message, for errors)
    Exception,  // FloCMS\Core\HttpException: HTTP 404 in /path/App.php:92
    PhpError,   // PHP Fatal error:  ..., PHP Parse error:  ...
    PhpWarning, // PHP Warning:  ..., PHP Notice:  ..., PHP Deprecated:  ...
    Trace,      // Stack trace:, #0 ..., "  thrown in ...": belongs to the line before
    Other,
};

struct ServerLine
{
    ServerLineKind kind = ServerLineKind::Other;
    QString time;    // as the server printed it: Sat Oct 24 12:53:57 2026
    QString text;    // the line without the time
    int status = 0;  // Request: the HTTP status; Exception: HTTP nnn of an HttpException
    QString method;  // Request
    QString path;    // Request
    QString message; // Request (after " - "), Exception, PhpError, PhpWarning
    QString file;    // Exception: where it was thrown
    int line = 0;
    bool operator==(const ServerLine &) const = default;
};

FLOSTUDIOCORE_EXPORT ServerLine readServerLine(const QString &line);

// A line worth pointing at: PHP errors and warnings, exceptions other
// than HttpException 4xx (a page not found is not a problem of the site),
// and requests answered with 5xx.
FLOSTUDIOCORE_EXPORT bool isProblem(const ServerLine &line);

enum class AppLogLevel { Debug, Info, Notice, Warning, Error, Critical, Alert, Emergency, Unknown };

struct AppLogLine
{
    bool continuation = false; // a further line of the message before (no "[time] level:")
    QString time;              // 2026-10-24 09:53:57, in PHP's time zone (often UTC)
    AppLogLevel level = AppLogLevel::Unknown;
    QString message;           // HTTP 404
    QString context;           // the JSON after the message, as written
    // An HttpException with a 4xx status: core logs every one, page not
    // found included; Studio doesn't count them as errors.
    int clientErrorStatus = 0;
    bool operator==(const AppLogLine &) const = default;
};

// One line of storage/logs/app.log: "[2026-10-24 09:53:57] error: HTTP 404 {...}".
FLOSTUDIOCORE_EXPORT AppLogLine readAppLogLine(const QString &line);

// Error and above, except the client errors above.
FLOSTUDIOCORE_EXPORT bool isProblem(const AppLogLine &line);

} // namespace FloStudio::Core
