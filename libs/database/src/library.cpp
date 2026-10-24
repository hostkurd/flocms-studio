#include <flostudio/database/library.h>

#include <flostudio/core/buildinfo.h>

namespace FloStudio::Database {

QString libraryName()
{
    return QStringLiteral("FloStudioDatabase");
}

QStringList dependencies()
{
    return {FloStudio::Core::libraryName()};
}

} // namespace FloStudio::Database
