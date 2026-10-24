#include <flostudio/core/logs.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace FloStudio::Core {

namespace {

int httpStatus(const QString &message)
{
    static const QRegularExpression http(QStringLiteral(R"(^HTTP (\d{3})\b)"));
    return http.match(message).captured(1).toInt();
}

} // namespace

ServerLine readServerLine(const QString &raw)
{
    // [Sat Oct 24 12:53:57 2026] (PHP's built-in server, cli-server SAPI)
    static const QRegularExpression time(QStringLiteral(R"(^\[((?:Mon|Tue|Wed|Thu|Fri|Sat|Sun) \w{3} +\d+ \d\d:\d\d:\d\d \d{4})\] )"));
    static const QRegularExpression started(QStringLiteral(R"(^PHP \S+ Development Server \(.+\) started$)"));
    static const QRegularExpression connection(QStringLiteral(R"(^\S+:\d+ (?:Accepted|Closing|Closed without sending a request\b.*)$)"));
    static const QRegularExpression request(QStringLiteral(R"(^\S+:\d+ \[(\d{3})\]: ([A-Z]+) (\S+)(?: - (.*))?$)"));
    static const QRegularExpression problem(QStringLiteral(R"(^PHP (Fatal error|Parse error|Recoverable fatal error|Warning|Notice|Deprecated): +(.*)$)"));
    static const QRegularExpression exception(QStringLiteral(R"(^([A-Za-z_][\w\\]*(?:Exception|Error)): (.*) in (.+):(\d+)$)"));
    static const QRegularExpression trace(QStringLiteral(R"(^(?:Stack trace:|#\d+ |\s+thrown in |Next [A-Za-z_\\]))"));

    ServerLine line;
    QString text = QString(raw).remove(u'\r');
    if (const auto match = time.match(text); match.hasMatch()) {
        line.time = match.captured(1);
        text = text.mid(match.capturedLength(0));
    }
    line.text = text;
    if (started.match(text).hasMatch()) {
        line.kind = ServerLineKind::Started;
    } else if (connection.match(text).hasMatch()) {
        line.kind = ServerLineKind::Connection;
    } else if (const auto match = request.match(text); match.hasMatch()) {
        line.kind = ServerLineKind::Request;
        line.status = match.captured(1).toInt();
        line.method = match.captured(2);
        line.path = match.captured(3);
        line.message = match.captured(4);
    } else if (const auto php = problem.match(text); php.hasMatch()) {
        const QString type = php.captured(1);
        line.kind = type.contains(QStringLiteral("error")) ? ServerLineKind::PhpError : ServerLineKind::PhpWarning;
        line.message = php.captured(2);
    } else if (const auto thrown = exception.match(text); thrown.hasMatch()) {
        line.kind = ServerLineKind::Exception;
        line.message = thrown.captured(2);
        line.file = thrown.captured(3);
        line.line = thrown.captured(4).toInt();
        if (thrown.captured(1).endsWith(QStringLiteral("HttpException")))
            line.status = httpStatus(line.message);
    } else if (line.time.isEmpty() && trace.match(text).hasMatch()) {
        line.kind = ServerLineKind::Trace;
    }
    return line;
}

bool isProblem(const ServerLine &line)
{
    switch (line.kind) {
    case ServerLineKind::PhpError:
    case ServerLineKind::PhpWarning:
        return true;
    case ServerLineKind::Exception:
        return line.status == 0 || line.status >= 500;
    case ServerLineKind::Request:
        return line.status >= 500;
    default:
        return false;
    }
}

AppLogLine readAppLogLine(const QString &raw)
{
    // core's Logger: sprintf("[%s] %s: %s%s\n", date('Y-m-d H:i:s'), level, message, ' ' . json(context))
    static const QRegularExpression head(QStringLiteral(R"(^\[(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)\] (\w+): (.*)$)"));
    AppLogLine line;
    const QString text = QString(raw).remove(u'\r');
    const auto match = head.match(text);
    if (!match.hasMatch()) {
        line.continuation = true;
        line.message = text;
        return line;
    }
    line.time = match.captured(1);
    const QString level = match.captured(2).toLower();
    static const QStringList levels{QStringLiteral("debug"),    QStringLiteral("info"),  QStringLiteral("notice"),
                                    QStringLiteral("warning"),  QStringLiteral("error"), QStringLiteral("critical"),
                                    QStringLiteral("alert"),    QStringLiteral("emergency")};
    const qsizetype index = levels.indexOf(level);
    line.level = index < 0 ? AppLogLevel::Unknown : static_cast<AppLogLevel>(index);

    // The context is the JSON object that ends the line: the first " {"
    // from which the rest of the line is a valid object (a message can
    // contain braces of its own before it).
    QString message = match.captured(3);
    QJsonObject context;
    for (qsizetype at = message.indexOf(QStringLiteral(" {")); at >= 0; at = message.indexOf(QStringLiteral(" {"), at + 1)) {
        QJsonParseError error{};
        const QJsonDocument document = QJsonDocument::fromJson(message.mid(at + 1).toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && document.isObject()) {
            line.context = message.mid(at + 1);
            context = document.object();
            message.truncate(at);
            break;
        }
    }
    line.message = message;
    const int status = httpStatus(message);
    if (status >= 400 && status < 500
        && context.value(QStringLiteral("type")).toString().endsWith(QStringLiteral("HttpException")))
        line.clientErrorStatus = status;
    return line;
}

bool isProblem(const AppLogLine &line)
{
    return !line.continuation && line.clientErrorStatus == 0 && line.level >= AppLogLevel::Error
           && line.level != AppLogLevel::Unknown;
}

} // namespace FloStudio::Core
