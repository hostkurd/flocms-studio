#include "appinfo.h"

#include <flostudio/core/buildinfo.h>
#include <flostudio/database/library.h>
#include <flostudio/security/library.h>

#include <QClipboard>
#include <QFontDatabase>
#include <QGuiApplication>

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
    return FloStudio::Core::toText(FloStudio::Core::currentBuildInfo());
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

void AppInfo::copyBuildText() const
{
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(buildText());
}
