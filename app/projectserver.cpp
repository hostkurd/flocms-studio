#include "projectserver.h"

#include "logmodel.h"

#include <flostudio/core/commands.h>
#include <flostudio/core/output.h>
#include <flostudio/core/process.h>
#include <flostudio/core/server.h>

#include <QProcess>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QUrl>

using namespace FloStudio::Core;

namespace {

constexpr int kStartSeconds = 20; // for the port to answer after "[OK] Serving"

} // namespace

ProjectServer::ProjectServer(QString projectDir, QString settingsDir, QObject *parent)
    : QObject(parent)
    , m_projectDir(std::move(projectDir))
    , m_settingsDir(std::move(settingsDir))
    , m_logs(new LogModel(this)) // a child of this
{
    m_probeTimer.setInterval(200);
    m_probeTimer.setSingleShot(true);
    connect(&m_probeTimer, &QTimer::timeout, this, &ProjectServer::probe);
    m_follow.setInterval(500);
    connect(&m_follow, &QTimer::timeout, this, &ProjectServer::followLog);
    if (!m_settingsDir.isEmpty()) { // not for a demo
        followLog();
        m_follow.start();
    }
}

ProjectServer::~ProjectServer()
{
    // The QProcess, a child, ends with its `php -S` when it is deleted (its
    // tracking stops the group). Ended here first, without telling this
    // object that is going away, so Qt has no running process to warn about.
    if (m_process) {
        QObject::disconnect(m_process, nullptr, this, nullptr);
        m_process->kill();
        m_process->waitForFinished(2000);
    }
    if (m_group > 0)
        recordServer(m_settingsDir, m_projectDir, 0);
}

QVariantMap ProjectServer::info() const
{
    return {{QStringLiteral("state"), m_state},
            {QStringLiteral("url"), m_url},
            {QStringLiteral("networkUrls"), m_url.isEmpty() || !m_networkHost ? QStringList() : networkUrls(m_url)},
            {QStringLiteral("startedAt"), m_startedAt.isValid() ? m_startedAt.toMSecsSinceEpoch() : 0},
            {QStringLiteral("php"), m_php},
            {QStringLiteral("error"), m_error},
            {QStringLiteral("detail"), m_detail}};
}

void ProjectServer::setState(const QString &state, const QString &error, const QString &detail)
{
    m_state = state;
    m_error = error;
    m_detail = detail;
    emit changed();
}

void ProjectServer::start(const Settings &settings)
{
    if (active() || m_settingsDir.isEmpty())
        return;
    CommandRequest request;
    request.program = Program::Flo;
    request.projectDir = m_projectDir;
    request.arguments = serveArguments(settings.serverPort, settings.serverNetwork);
    const auto command = prepareCommand(settings, request);
    if (!command) {
        const QString code = command.error().code == u"unsafe_argument" ? QStringLiteral("php_unsafe") : command.error().code;
        setState(QStringLiteral("failed"), code, command.error().detail);
        return;
    }
    m_url.clear();
    m_php.clear();
    m_output.clear();
    m_startedAt = {};
    m_stopping = false;
    m_networkHost = settings.serverNetwork;
    m_logs->addStudioLine(command->shown, QStringLiteral("command"));
    setState(QStringLiteral("starting"));
    m_process = startStreaming(
        command.value(), this, [this](const OutputLine &out) { line(out.text, out.error); },
        [this](int exitCode, bool crashed) { ended(exitCode, crashed); });
}

void ProjectServer::stop()
{
    if (!m_process || m_state == u"stopping")
        return;
    m_stopping = true;
    m_probeTimer.stop();
    setState(QStringLiteral("stopping"));
    stopProcess(m_process);
}

void ProjectServer::line(const QString &text, bool error)
{
    if (m_state == u"starting") {
        m_output << text;
        if (m_url.isEmpty()) {
            if (const QString url = serveUrl(text); !url.isEmpty()) {
                m_url = localUrl(url);
                m_probeDeadline = QDateTime::currentDateTime().addSecs(kStartSeconds);
                probe();
                emit changed();
            }
        }
    }
    // PHP 8.3.6 Development Server (http://127.0.0.1:8000) started
    static const QRegularExpression started(QStringLiteral("PHP (\\d+\\.\\d+\\.\\d+\\S*) Development Server"));
    if (const auto match = started.match(text); match.hasMatch() && m_php.isEmpty()) {
        m_php = match.captured(1);
        emit changed();
    }
    // "[OK] Serving ..." and "Port 8000 is in use." are Studio's to say
    if (text.startsWith(QStringLiteral("[OK]")) || text.startsWith(QStringLiteral("Port "))) {
        m_logs->addStudioLine(text.startsWith(QStringLiteral("[OK]")) ? text.mid(4).trimmed() : text,
                              QStringLiteral("info"));
        return;
    }
    Q_UNUSED(error)
    m_logs->addServerLine(text);
}

void ProjectServer::probe()
{
    if (m_state != u"starting" || m_url.isEmpty())
        return;
    if (QDateTime::currentDateTime() > m_probeDeadline) {
        m_stopping = true;
        setState(QStringLiteral("failed"), QStringLiteral("server_no_answer"), m_url);
        if (m_process)
            stopProcess(m_process);
        return;
    }
    if (!m_probe) {
        m_probe = new QTcpSocket(this); // a child of this; deleted once answered
        connect(m_probe, &QTcpSocket::connected, this, [this] {
            m_probe->abort();
            m_probe->deleteLater();
            m_probe = nullptr;
            if (m_state != u"starting")
                return;
            m_startedAt = QDateTime::currentDateTime();
            if (m_process) {
                m_group = m_process->processId();
                recordServer(m_settingsDir, m_projectDir, m_group);
            }
            m_logs->addStudioLine(m_url, QStringLiteral("started"));
            setState(QStringLiteral("running"));
        });
        connect(m_probe, &QTcpSocket::errorOccurred, this, [this] {
            if (m_probe)
                m_probe->abort();
            m_probeTimer.start(); // again shortly
        });
    }
    const QUrl url(m_url);
    m_probe->abort();
    m_probe->connectToHost(url.host(), static_cast<quint16>(url.port()));
}

void ProjectServer::ended(int exitCode, bool crashed)
{
    m_process = nullptr; // startStreaming deletes it after this
    m_probeTimer.stop();
    if (m_probe) {
        m_probe->abort();
        m_probe->deleteLater();
        m_probe = nullptr;
    }
    if (m_group > 0) {
        recordServer(m_settingsDir, m_projectDir, 0);
        m_group = 0;
    }
    m_logs->flush();
    if (m_state == u"failed") {
        emit changed();
        return;
    }
    if (m_stopping) {
        m_logs->addStudioLine(QString(), QStringLiteral("stopped"));
        setState(QStringLiteral("stopped"));
        return;
    }
    // It ended on its own: the message flocms-cli printed, or the exit code
    const QString message = errorMessage(m_output.join(u'\n'));
    const QString detail = message.isEmpty() ? (crashed ? QStringLiteral("crashed") : QStringLiteral("exit %1").arg(exitCode))
                                             : message;
    m_logs->addStudioLine(detail, QStringLiteral("problem"));
    setState(QStringLiteral("failed"), m_state == u"running" ? QStringLiteral("server_ended") : QStringLiteral("server_failed"), detail);
}

void ProjectServer::followLog()
{
    const QString path = appLogPath(m_projectDir);
    if (m_logOffset < 0) {
        // Opened: the last lines that are already there
        const LogRead tail = readLogTail(path, 200);
        m_logOffset = tail.offset;
        for (const QString &text : tail.lines)
            m_logs->addAppLogLine(text);
        return;
    }
    const LogRead read = readLogFrom(path, m_logOffset);
    m_logOffset = read.offset;
    for (const QString &text : read.lines)
        m_logs->addAppLogLine(text);
}

void ProjectServer::showDemo(const QVariantMap &server, const QVariantList &lines)
{
    m_state = server.value(QStringLiteral("state"), QStringLiteral("stopped")).toString();
    m_url = server.value(QStringLiteral("url")).toString();
    m_php = server.value(QStringLiteral("php")).toString();
    m_error = server.value(QStringLiteral("error")).toString();
    m_detail = server.value(QStringLiteral("detail")).toString();
    m_networkHost = server.value(QStringLiteral("network")).toBool();
    const int minutes = server.value(QStringLiteral("startedMinutesAgo"), -1).toInt();
    m_startedAt = minutes >= 0 ? QDateTime::currentDateTime().addSecs(-60LL * minutes) : QDateTime();
    for (const QVariant &row : lines) {
        const QVariantMap entry = row.toMap();
        const QString source = entry.value(QStringLiteral("source")).toString();
        const QString text = entry.value(QStringLiteral("text")).toString();
        if (source == u"app")
            m_logs->addAppLogLine(text);
        else if (source == u"studio")
            m_logs->addStudioLine(text, entry.value(QStringLiteral("level"), QStringLiteral("info")).toString());
        else
            m_logs->addServerLine(text);
    }
    m_logs->flush();
    emit changed();
}

void ProjectServer::noteLeftover(qint64 group)
{
    m_logs->addStudioLine(QString::number(group), QStringLiteral("leftover"));
}
