#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// Build details for the placeholder window (and the About box later).
class AppInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString buildText READ buildText CONSTANT)
    Q_PROPERTY(QStringList libraries READ libraries CONSTANT)

public:
    explicit AppInfo(QObject *parent = nullptr);

    QString version() const;
    QString buildText() const;
    QStringList libraries() const;

    Q_INVOKABLE void copyBuildText() const;
};
