#include "appinfo.h"

#include <flostudio/core/buildinfo.h>
#include <flostudio/database/library.h>
#include <flostudio/security/library.h>

#include <QClipboard>
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

void AppInfo::copyBuildText() const
{
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(buildText());
}
