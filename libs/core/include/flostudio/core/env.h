#pragma once

#include <flostudio/core/export.h>
#include <flostudio/core/result.h>

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>

namespace FloStudio::Core {

// A project's .env file, read the way FloCMS reads it. There are two
// readers in FloCMS:
//
//   - flocms-core's Env::load: the website (public/index.php), and every
//     `php flo` command that boots the project (admin:*, route:list, ...)
//   - flocms-cli's Project::dotenv: `php flo` commands before they boot it
//     (migrate, db:*, doctor)
//
// Both: one KEY=value per line, lines starting with # are comments, the
// key and the value are trimmed, one pair of surrounding quotes (" or ')
// is removed, nothing else (no escapes, no inline comments: `a #b` is the
// value "a #b"). A variable already set in the real environment wins over
// the file in both (Studio's commands run without them, see commands.h).
//
// They differ when a key is written twice: Env::load keeps the first
// value, Project::dotenv the last, and a command that boots sees the first
// one from then on. So `php flo migrate` and `php flo admin:doctor` can use
// different databases. Studio shows the first value (what the website
// uses), points at such keys (duplicateKeys) and, when it writes one,
// leaves a single line.
//
// A UTF-8 byte order mark stays part of the first key, as in PHP: the key
// FloCMS reads is then not APP_NAME but "\uFEFFAPP_NAME".

// The values as the website sees them (first line of a key wins). Studio
// shows these.
FLOSTUDIOCORE_EXPORT QMap<QString, QString> parseEnv(const QString &text);

// The values as Project::dotenv sees them (last line of a key wins).
FLOSTUDIOCORE_EXPORT QMap<QString, QString> parseEnvForCommands(const QString &text);

// Keys that appear on more than one line, in the order first seen.
FLOSTUDIOCORE_EXPORT QStringList duplicateKeys(const QString &text);

// A .env file's bytes as text, its byte order mark kept as U+FEFF.
FLOSTUDIOCORE_EXPORT QString envText(const QByteArray &file);

// Reads dir/.env with envText(); empty when there is no file.
FLOSTUDIOCORE_EXPORT QString readEnvFile(const QString &dir);

// The file starts with a byte order mark (its first key is not what it
// looks like; see above).
FLOSTUDIOCORE_EXPORT bool hasByteOrderMark(const QString &text);

// A yes/no setting such as APP_DEBUG, read as core's ErrorHandler does
// (PHP's FILTER_VALIDATE_BOOLEAN): 1, true, on, yes are true; 0, false,
// off, no and empty are false; any other value, or none, gives fallback.
FLOSTUDIOCORE_EXPORT bool envFlag(const QMap<QString, QString> &env, const QString &key, bool fallback);

// APP_ENV is production, prod or live, or not set (FloCMS's default is
// production). Commands such as migrate then need --force.
FLOSTUDIOCORE_EXPORT bool isProduction(const QMap<QString, QString> &env);

// The keys Studio may write (the new project wizard and the Database tab).
// APP_KEY is not one: `php flo key:generate` writes it.
FLOSTUDIOCORE_EXPORT const QStringList &writableEnvKeys();

// A value written so that both readers give it back exactly. Quotes are
// added only when they are needed. Error codes: env_value (a line break,
// which no .env line can hold).
FLOSTUDIOCORE_EXPORT Result<QString> envLineValue(const QString &value);

// The file (its bytes) with the values set: the first line of each key
// gets the new value, later lines of the same key are removed, a missing
// key is added at the end. Every other line stays byte for byte, line
// ending included; added lines use CRLF when the file has any. Error
// codes: env_key (not in writableEnvKeys), env_value.
FLOSTUDIOCORE_EXPORT Result<QByteArray> setEnvValues(const QByteArray &file,
                                                     const QList<QPair<QString, QString>> &values);

// setEnvValues on dir/.env, written in one step. A project without .env
// starts from its .env.example, as `composer create-project` does.
// Error codes: env_key, env_value, env_write.
FLOSTUDIOCORE_EXPORT Result<Done> writeEnvValues(const QString &dir, const QList<QPair<QString, QString>> &values);

} // namespace FloStudio::Core
