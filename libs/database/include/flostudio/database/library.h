#pragma once

#include <flostudio/database/export.h>

#include <QString>
#include <QStringList>

// FloStudioDatabase: stage 1 only sets up the library and its link to Core.
// Its real contents arrive in later stages (design note D5).
namespace FloStudio::Database {

FLOSTUDIODATABASE_EXPORT QString libraryName();

// The libraries this one is linked with, nearest first: FloStudioCore.
FLOSTUDIODATABASE_EXPORT QStringList dependencies();

} // namespace FloStudio::Database
