#pragma once

#include <flostudio/core/result.h>
#include <flostudio/security/export.h>

#include <QString>
#include <QStringList>

#include <functional>

class QObject;

// Passwords and keys (stage 6, design note D5).
//
// - What Studio shows (command output, logs) never holds a password or key:
//   redact() replaces them.
// - Database passwords can be saved in the system's own secure store:
//   Windows Credential Manager, the macOS Keychain, the Secret Service
//   (GNOME Keyring, KWallet) on Linux. Never in a file of Studio's: where no
//   such store runs, nothing is saved.
// - The first admin's password goes to `php flo user:create` on its input
//   only (Core's Command::input), never in an argument or a log.
namespace FloStudio::Security {

// The text with every secret in it replaced by •••••••• (secrets shorter
// than 3 characters are left: they would match everywhere).
FLOSTUDIOSECURITY_EXPORT QString redact(const QString &text, const QStringList &secrets);

// The values in a .env text that are secrets: APP_KEY, DB_PASSWORD, and any
// key ending in _SECRET, _PASSWORD, _TOKEN or _KEY.
FLOSTUDIOSECURITY_EXPORT QStringList envSecrets(const QString &envText);

// The name a database password is saved under: mysql://user@host:port.
FLOSTUDIOSECURITY_EXPORT QString databaseAccount(const QString &host, int port, const QString &user);

// The secure store can be used here (false on Linux without a Secret
// Service, and under QStandardPaths' test mode unless
// FLOSTUDIO_KEYCHAIN_TESTS=1).
FLOSTUDIOSECURITY_EXPORT bool secureStoreAvailable();

// Saving, reading and forgetting a password in the secure store. Each
// reports once, on `context`'s thread, from the event loop. Error codes:
// keychain_unavailable, keychain_denied (the person said no), keychain_failed,
// keychain_missing (read: nothing saved).
using SecretDone = std::function<void(const FloStudio::Core::Result<QString> &secret)>;
FLOSTUDIOSECURITY_EXPORT void savePassword(const QString &account, const QString &password, QObject *context, SecretDone done);
FLOSTUDIOSECURITY_EXPORT void readPassword(const QString &account, QObject *context, SecretDone done);
FLOSTUDIOSECURITY_EXPORT void forgetPassword(const QString &account, QObject *context, SecretDone done);

} // namespace FloStudio::Security
