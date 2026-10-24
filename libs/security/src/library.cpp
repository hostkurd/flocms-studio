#include <flostudio/security/library.h>

#include <flostudio/core/buildinfo.h>

namespace FloStudio::Security {

QString libraryName()
{
    return QStringLiteral("FloStudioSecurity");
}

QStringList dependencies()
{
    return {FloStudio::Core::libraryName()};
}

} // namespace FloStudio::Security
