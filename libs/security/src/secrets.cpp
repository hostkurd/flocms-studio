#include <flostudio/security/secrets.h>

#include <flostudio/core/env.h>

#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include <qtkeychain/keychain.h>

#include <algorithm>

using FloStudio::Core::Error;
using FloStudio::Core::Result;

namespace FloStudio::Security {

namespace {

// The secure store's entries: one service for Studio
QString service()
{
    return QStringLiteral("FloCMS Studio");
}

Error keychainError(const QKeychain::Job &job)
{
    switch (job.error()) {
    case QKeychain::NoBackendAvailable:
    case QKeychain::NotImplemented:
        return Error{QStringLiteral("keychain_unavailable"), job.errorString()};
    case QKeychain::AccessDeniedByUser:
    case QKeychain::AccessDenied:
        return Error{QStringLiteral("keychain_denied"), job.errorString()};
    case QKeychain::EntryNotFound:
        return Error{QStringLiteral("keychain_missing"), job.errorString()};
    default:
        break;
    }
    return Error{QStringLiteral("keychain_failed"), job.errorString()};
}

// Reports from the event loop, also when the store isn't there at all
void later(QObject *context, const SecretDone &done, const Result<QString> &result)
{
    QTimer::singleShot(0, context, [done, result] { done(result); });
}

void run(QKeychain::Job *job, QObject *context, SecretDone done, bool read)
{
    job->setAutoDelete(true); // QtKeychain deletes it after finished
    job->setInsecureFallback(false); // never a plain file instead
    QObject::connect(job, &QKeychain::Job::finished, context, [done = std::move(done), read](QKeychain::Job *finished) {
        if (finished->error() != QKeychain::NoError) {
            done(keychainError(*finished));
            return;
        }
        const auto *reader = read ? qobject_cast<QKeychain::ReadPasswordJob *>(finished) : nullptr;
        done(reader ? reader->textData() : QString());
    });
    job->start();
}

} // namespace

QString redact(const QString &text, const QStringList &secrets)
{
    QString out = text;
    QStringList sorted;
    for (const QString &secret : secrets) {
        if (secret.size() >= 3 && !sorted.contains(secret))
            sorted << secret;
    }
    // Longest first: a secret that contains another is replaced whole
    std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) { return a.size() > b.size(); });
    for (const QString &secret : std::as_const(sorted))
        out.replace(secret, QStringLiteral("••••••••"));
    return out;
}

QStringList envSecrets(const QString &envText)
{
    static const QRegularExpression secretKey(QStringLiteral("(^APP_KEY$|^DB_PASSWORD$|_SECRET$|_PASSWORD$|_TOKEN$|_KEY$)"));
    // Both readers: a key written twice has a value the website sees and
    // one some commands see, and neither may show
    QStringList secrets;
    for (const auto &values : {FloStudio::Core::parseEnv(envText), FloStudio::Core::parseEnvForCommands(envText)}) {
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            if (secretKey.match(it.key()).hasMatch() && !it.value().isEmpty() && !secrets.contains(it.value()))
                secrets << it.value();
        }
    }
    return secrets;
}

QString databaseAccount(const QString &host, int port, const QString &user)
{
    return QStringLiteral("mysql://%1@%2:%3").arg(user.trimmed(), host.trimmed()).arg(port);
}

bool secureStoreAvailable()
{
    // Tests (QStandardPaths' test mode) never touch the person's real store
    if (QStandardPaths::isTestModeEnabled() && qEnvironmentVariable("FLOSTUDIO_KEYCHAIN_TESTS") != u"1")
        return false;
    return QKeychain::isAvailable();
}

void savePassword(const QString &account, const QString &password, QObject *context, SecretDone done)
{
    if (!secureStoreAvailable()) {
        later(context, done, Error{QStringLiteral("keychain_unavailable"), {}});
        return;
    }
    auto *job = new QKeychain::WritePasswordJob(service(), context); // deletes itself when finished
    job->setKey(account);
    job->setTextData(password);
    run(job, context, std::move(done), false);
}

void readPassword(const QString &account, QObject *context, SecretDone done)
{
    if (!secureStoreAvailable()) {
        later(context, done, Error{QStringLiteral("keychain_unavailable"), {}});
        return;
    }
    auto *job = new QKeychain::ReadPasswordJob(service(), context); // deletes itself when finished
    job->setKey(account);
    run(job, context, std::move(done), true);
}

void forgetPassword(const QString &account, QObject *context, SecretDone done)
{
    if (!secureStoreAvailable()) {
        later(context, done, Error{QStringLiteral("keychain_unavailable"), {}});
        return;
    }
    auto *job = new QKeychain::DeletePasswordJob(service(), context); // deletes itself when finished
    job->setKey(account);
    run(job, context, std::move(done), false);
}

} // namespace FloStudio::Security
