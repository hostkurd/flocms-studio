#pragma once

#include <flostudio/core/packages.h>
#include <flostudio/core/settings.h>

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QProcess;

// One project's Packages tab (stage 8, board 8): hostkurd's packages from
// composer.lock with what `composer outdated` says is newer, updating one,
// and adding the API or the admin panel (libs/core packages.h). The
// commands of an update run one after the other, with their output live;
// one that fails stops the rest. Belongs to Studio, which outlives it.
class ProjectPackages : public QObject
{
    Q_OBJECT
    // [{name, version, latest, status (current, update, major, unknown,
    //   missing), repository, upgrading, description, canAdd}]
    Q_PROPERTY(QVariantList packages READ packages NOTIFY packagesChanged)
    // ["psr/log 3.0.2", ...]: the rest of composer.lock
    Q_PROPERTY(QStringList others READ others NOTIFY packagesChanged)
    // composer outdated: {state (idle, running, done, failed), checkedAt,
    //  error, detail, message}
    Q_PROPERTY(QVariantMap check READ check NOTIFY packagesChanged)
    // An update or an addition: {kind (update, add), name, from, to,
    //  state (running, done, failed), steps: [{shown, state}], lines,
    //  error, detail, message, exitCode}
    Q_PROPERTY(QVariantMap run READ run NOTIFY runChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY runChanged)

public:
    ProjectPackages(QString projectDir, const FloStudio::Core::Settings *settings, QObject *parent = nullptr);
    ~ProjectPackages() override;
    ProjectPackages(const ProjectPackages &) = delete;
    ProjectPackages &operator=(const ProjectPackages &) = delete;

    QVariantList packages() const;
    QStringList others() const { return m_others; }
    QVariantMap check() const { return m_check; }
    QVariantMap run() const { return m_run; }
    bool busy() const { return m_process != nullptr || m_check.value(QStringLiteral("state")).toString() == u"running"; }

    // What composer.lock lists (Studio reads it): [{name, version}]
    void setInstalled(const QVariantList &packages, const QVariantList &others);

    // Asks Composer what is newer (it asks Packagist).
    Q_INVOKABLE void checkUpdates();
    // What an update or an addition runs, before it runs: {kind, name,
    // from, to, steps: [shown], changelog, upgrading}.
    Q_INVOKABLE QVariantMap planUpdate(const QString &name) const;
    Q_INVOKABLE QVariantMap planAdd(const QString &name) const;
    Q_INVOKABLE void update(const QString &name);
    Q_INVOKABLE void add(const QString &name);
    Q_INVOKABLE void clearRun();

    // A demo: composer outdated's output and a run from the demo file.
    void showDemo(const QVariantMap &demo);

signals:
    void packagesChanged();
    void runChanged();
    // composer.json, composer.lock and the database changed: Studio reads
    // the project again
    void changedProject();

private:
    void takeOutdated(const QString &out, const QString &err, int exitCode);
    void start(const QString &kind, const QString &name, const QString &from, const QString &to,
               const QList<FloStudio::Core::PackageStep> &steps);
    void runStep(int index);
    void setStepState(int index, const QString &state);
    bool demo() const { return m_settings == nullptr; }

    QString m_projectDir;
    const FloStudio::Core::Settings *m_settings; // Studio's; null for a demo
    QVariantList m_installed;                    // [{name, version}] from composer.lock
    QStringList m_others;
    QList<FloStudio::Core::PackageStatus> m_status; // from composer outdated
    QVariantMap m_check{{QStringLiteral("state"), QStringLiteral("idle")}};
    int m_checkRun = 0;

    QVariantMap m_run;
    QList<FloStudio::Core::PackageStep> m_steps;
    QStringList m_lines;
    QStringList m_errors;
    QProcess *m_process = nullptr; // a child of this while a step runs
};
