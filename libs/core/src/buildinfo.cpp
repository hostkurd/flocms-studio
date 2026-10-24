#include <flostudio/core/buildinfo.h>
#include <flostudio/core/version.h>

#include <QStringList>
#include <QSysInfo>
#include <QtGlobal>

namespace FloStudio::Core {

namespace {

QString compilerName()
{
#if defined(__clang__)
    return QStringLiteral("Clang %1.%2.%3").arg(__clang_major__).arg(__clang_minor__).arg(__clang_patchlevel__);
#elif defined(_MSC_VER)
    return QStringLiteral("MSVC %1.%2").arg(_MSC_VER / 100).arg(_MSC_VER % 100);
#elif defined(__GNUC__)
    return QStringLiteral("GCC %1.%2.%3").arg(__GNUC__).arg(__GNUC_MINOR__).arg(__GNUC_PATCHLEVEL__);
#else
    return QStringLiteral("unknown compiler");
#endif
}

} // namespace

BuildInfo currentBuildInfo()
{
    BuildInfo info;
    info.version = QStringLiteral(FLOSTUDIO_VERSION);
    info.qtBuilt = QStringLiteral(QT_VERSION_STR);
    info.qtRunning = QString::fromLatin1(qVersion());
    info.qtStatic = FLOSTUDIO_QT_STATIC_BUILD != 0;
    info.sharedLibraries = FLOSTUDIO_SHARED_LIBS_BUILD != 0;
    info.compiler = compilerName();
    info.architecture = QSysInfo::buildCpuArchitecture();
    return info;
}

QString toText(const BuildInfo &info)
{
    return QStringList{
        QStringLiteral("FloCMS Studio %1").arg(info.version),
        QStringLiteral("Qt %1 (%2), running %3")
            .arg(info.qtBuilt, info.qtStatic ? QStringLiteral("static") : QStringLiteral("shared"), info.qtRunning),
        QStringLiteral("FloStudio libraries: %1")
            .arg(info.sharedLibraries ? QStringLiteral("shared") : QStringLiteral("static")),
        QStringLiteral("%1, %2").arg(info.compiler, info.architecture),
    }.join(QLatin1Char('\n'));
}

QString libraryName()
{
    return QStringLiteral("FloStudioCore");
}

} // namespace FloStudio::Core
