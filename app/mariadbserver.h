#pragma once

#include <flostudio/core/dbserver.h>
#include <flostudio/core/downloads.h>
#include <flostudio/core/settings.h>

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// Studio's own MariaDB server for the screens (MariaDB design note D5–D12):
// which programs Studio has (its downloads; Homebrew's on macOS), the one
// in use, starting and stopping it, root's password in the secure store.
// Core's dbserver.h does the work; this keeps the state for QML.
//
// state: none (no program), stopped, starting, running, stopping, failed
// (error, detail). phase while starting: check, setup, start, password.
class MariadbServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    // The programs: [{series, version, path, origin (studio, homebrew)}], newest first
    Q_PROPERTY(QVariantList programs READ programs NOTIFY changed)
    Q_PROPERTY(QString series READ series NOTIFY changed)
    Q_PROPERTY(QString version READ version NOTIFY changed)
    Q_PROPERTY(QString program READ program NOTIFY changed)
    Q_PROPERTY(QString origin READ origin NOTIFY changed)
    Q_PROPERTY(QString dataFolder READ dataFolder NOTIFY changed)
    Q_PROPERTY(int port READ port NOTIFY changed)
    // A free port to offer when the chosen one is taken (0: none known)
    Q_PROPERTY(int freePort READ freePort NOTIFY changed)
    Q_PROPERTY(bool autoStart READ autoStart NOTIFY changed)
    Q_PROPERTY(qint64 startedAt READ startedAt NOTIFY changed)
    // root's password while Show is on; empty otherwise
    Q_PROPERTY(QString shownPassword READ shownPassword NOTIFY changed)
    Q_PROPERTY(bool initialized READ initialized NOTIFY changed)
    // What the last start printed (commands and output), for the screen
    Q_PROPERTY(QStringList lines READ lines NOTIFY changed)
    // Where the secure store is: Windows Credential Manager, macOS Keychain, the keyring
    Q_PROPERTY(bool secureStore READ secureStore CONSTANT)
    // Linux: the package with libaio for this system (libaio1t64 on Ubuntu
    // 24.04+ and Debian 13+, else libaio1)
    Q_PROPERTY(QString libaioPackage READ libaioPackage CONSTANT)

public:
    explicit MariadbServer(QObject *parent = nullptr);
    ~MariadbServer() override;
    MariadbServer(const MariadbServer &) = delete;
    MariadbServer &operator=(const MariadbServer &) = delete;

    // Studio's settings (owned by Studio) and its packages; saving goes
    // back through Studio (settingsChanged() → Studio saves)
    void attach(FloStudio::Core::Settings *settings, const QList<FloStudio::Core::ToolPackage> &packages,
                const QString &platform);
    // Looks for the programs again (after a download), on a worker thread
    void refresh();
    // A demo: the state from a JSON object, nothing started
    void showDemo(const QVariantMap &demo);

    QString state() const { return m_state; }
    QString phase() const { return m_phase; }
    QString error() const { return m_error; }
    QString detail() const { return m_detail; }
    QVariantList programs() const { return m_programs; }
    QString series() const;
    QString version() const;
    QString program() const;
    QString origin() const;
    QString dataFolder() const;
    int port() const;
    int freePort() const { return m_freePort; }
    bool autoStart() const;
    qint64 startedAt() const { return m_startedAt.isValid() ? m_startedAt.toMSecsSinceEpoch() : 0; }
    QString shownPassword() const { return m_shownPassword; }
    bool initialized() const;
    QStringList lines() const { return m_lines; }
    static bool secureStore();
    static QString libaioPackage();
    // The project's database is on Studio's server (made there by the
    // wizard, or marked so)
    Q_INVOKABLE bool usedBy(const QString &projectPath) const;
    void addProject(const QString &projectPath);
    bool active() const { return m_state == u"starting" || m_state == u"running" || m_state == u"stopping"; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    // The port taken: use freePort and start
    Q_INVOKABLE void startOnFreePort();
    Q_INVOKABLE void setPort(int port);
    Q_INVOKABLE void setAutoStart(bool on);
    Q_INVOKABLE void setSeries(const QString &series);
    Q_INVOKABLE void showPassword(bool on);
    Q_INVOKABLE void copyPassword();
    Q_INVOKABLE void openLog();
    Q_INVOKABLE void openDataFolder();
    // Deletes Studio's download of the series in use (stopped first), and
    // its databases too when asked. Never Homebrew's.
    Q_INVOKABLE void remove(bool databasesToo);
    Q_INVOKABLE void copyText(const QString &text);

    // The account root's password is saved under in the secure store
    static QString passwordAccount(const QString &series);

signals:
    void changed();
    void settingsChanged(); // a setting changed: Studio saves
    void programsFound();   // after refresh()

private:
    void setState(const QString &state, const QString &error = {}, const QString &detail = {});
    void addLine(const QString &text);
    void launch(const QString &password);
    QVariantMap current() const;
    FloStudio::Core::DatabaseServerFiles files() const;

    FloStudio::Core::Settings *m_settings = nullptr;
    QList<FloStudio::Core::ToolPackage> m_packages;
    QString m_platform;
    QVariantList m_programs;
    QObject *m_server = nullptr; // Core's, a child of this while it runs; cleared when it goes
    int m_run = 0;
    QString m_state = QStringLiteral("none");
    QString m_phase;
    QString m_error;
    QString m_detail;
    QString m_password; // root's, read from the secure store, while running
    QString m_shownPassword;
    int m_freePort = 0;
    QDateTime m_startedAt;
    QStringList m_lines;
    bool m_demo = false;
    QVariantMap m_demoInfo;
    QString m_activeSeries; // the series that runs (kept while active)
};
