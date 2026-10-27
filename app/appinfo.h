#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// Build details for the About page (and `--version`).
class AppInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString buildText READ buildText CONSTANT)
    Q_PROPERTY(QStringList libraries READ libraries CONSTANT)
    // The system's fixed-width font ("monospace" is not a font on macOS).
    Q_PROPERTY(QString monoFont READ monoFont CONSTANT)
    // LICENSE.txt and THIRD-PARTY-NOTICES.txt as shipped with this build
    // (next to Studio.exe, in the app's Resources, in the Linux package's
    // share/doc); empty when this build has none (a build folder)
    Q_PROPERTY(QString licenseFile READ licenseFile CONSTANT)
    Q_PROPERTY(QString noticesFile READ noticesFile CONSTANT)

public:
    explicit AppInfo(QObject *parent = nullptr);

    QString version() const;
    QString buildText() const;
    QStringList libraries() const;
    QString monoFont() const;
    QString licenseFile() const;
    QString noticesFile() const;

    // Version, Qt, libraries, compiler, TLS backends and the system it runs
    // on: what `--version` prints and "Copy version info" copies
    static QString report();

    Q_INVOKABLE void copyBuildText() const;
    Q_INVOKABLE void openFile(const QString &path) const;
};
