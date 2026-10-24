#pragma once

#include <flostudio/security/export.h>

#include <QString>
#include <QStringList>

// FloStudioSecurity: stage 1 only sets up the library and its link to Core.
// Its real contents arrive in later stages (design note D5).
namespace FloStudio::Security {

FLOSTUDIOSECURITY_EXPORT QString libraryName();

// The libraries this one is linked with, nearest first: FloStudioCore.
FLOSTUDIOSECURITY_EXPORT QStringList dependencies();

} // namespace FloStudio::Security
