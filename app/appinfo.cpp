#include "appinfo.h"

#include <flostudio/core/buildinfo.h>
#include <flostudio/database/library.h>
#include <flostudio/security/library.h>

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QSysInfo>
#include <QUrl>
#include <QtNetwork/qtnetworkglobal.h>
#if QT_CONFIG(ssl)
#include <QSslSocket>
#endif

namespace {

// Where a release keeps a text file that ships with it
QString shippedFile(const QString &name)
{
    const QDir dir(QCoreApplication::applicationDirPath());
    for (const QString &folder : {QStringLiteral("."), QStringLiteral("../Resources"),
                                  QStringLiteral("../share/doc/flocms-studio")}) {
        const QString path = QDir::cleanPath(dir.filePath(folder + u'/' + name));
        if (QFileInfo(path).isFile())
            return QDir::toNativeSeparators(path);
    }
    return {};
}

} // namespace

AppInfo::AppInfo(QObject *parent)
    : QObject(parent)
{
}

QString AppInfo::version() const
{
    return FloStudio::Core::currentBuildInfo().version;
}

QString AppInfo::buildText() const
{
    return report();
}

QString AppInfo::report()
{
    // The TLS backends (plugins) Studio's downloads can use: a release
    // build with Qt inside must have one (CI checks the TLS line)
#if QT_CONFIG(ssl)
    const QStringList tls = QSslSocket::availableBackends();
#else
    const QStringList tls;
#endif
    return FloStudio::Core::toText(FloStudio::Core::currentBuildInfo())
           + QStringLiteral("\nTLS: ") + (tls.isEmpty() ? QStringLiteral("none") : tls.join(QStringLiteral(", ")))
           + QStringLiteral("\nSystem: ") + QSysInfo::prettyProductName() + QStringLiteral(", ")
           + QSysInfo::currentCpuArchitecture();
}

QStringList AppInfo::libraries() const
{
    return {FloStudio::Core::libraryName(), FloStudio::Database::libraryName(),
            FloStudio::Security::libraryName()};
}

QString AppInfo::monoFont() const
{
    return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

QString AppInfo::licenseFile() const
{
    return shippedFile(QStringLiteral("LICENSE.txt"));
}

QString AppInfo::noticesFile() const
{
    return shippedFile(QStringLiteral("THIRD-PARTY-NOTICES.txt"));
}

void AppInfo::copyBuildText() const
{
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(buildText());
}

void AppInfo::openFile(const QString &path) const
{
    if (!path.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
