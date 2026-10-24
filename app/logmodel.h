#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QTimer>

// The Logs tab (stage 4, D10): what the server prints and what core writes
// to storage/logs/app.log, one list in the order the lines arrive. Lines
// are classified when added (libs/core logs.h), batched every 50 ms, and
// shown through a filter (source, problems only, search) in a ListView,
// which only makes the rows on screen, so thousands of lines stay smooth.
// The oldest lines go once there are more than `limit`.
class LogModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY filterChanged) // all, server, app
    Q_PROPERTY(bool problemsOnly READ problemsOnly WRITE setProblemsOnly NOTIFY filterChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY filterChanged)
    Q_PROPERTY(int total READ total NOTIFY countsChanged)       // every line kept
    Q_PROPERTY(int problems READ problems NOTIFY countsChanged) // problem lines kept
    Q_PROPERTY(int shown READ shown NOTIFY countsChanged)       // lines the filter shows

public:
    enum Role {
        TextRole = Qt::UserRole + 1,
        SourceRole,       // server, app, studio
        LevelRole,        // problem, warning, info, request, quiet
        TimeRole,         // 12:53:57, or empty
        ContinuationRole, // a further line of the one before (a stack trace)
    };

    struct Entry
    {
        QString text;
        QString source;
        QString level;
        QString time;
        bool continuation = false;
        QDateTime appStamp; // app.log: its time as written, in PHP's zone
    };

    explicit LogModel(QObject *parent = nullptr, int limit = 10000);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // A line as `php flo serve` printed it (stdout or stderr). Connection
    // lines (Accepted, Closing) are left out: one per request, nothing to read.
    void addServerLine(const QString &line);
    // A line of storage/logs/app.log.
    void addAppLogLine(const QString &line);
    // What Studio itself says (Started, Stopped, a command it ran).
    void addStudioLine(const QString &text, const QString &level = QStringLiteral("info"));
    // Shown now rather than in the next batch (tests, demos).
    void flush();
    // core's Logger writes app.log times in PHP's time zone (date.timezone,
    // UTC when php.ini sets none); the server and Studio use this
    // computer's. With PHP's offset known, app.log times are shown in this
    // computer's zone too, so one column reads the same.
    void setAppTimeOffset(int secondsEastOfUtc);

    QString source() const { return m_source; }
    void setSource(const QString &source);
    bool problemsOnly() const { return m_problemsOnly; }
    void setProblemsOnly(bool only);
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    int total() const { return static_cast<int>(m_entries.size()); }
    int problems() const { return m_problems; }
    int shown() const { return static_cast<int>(m_visible.size()); }

    // The shown lines as text, one per line, for Copy.
    Q_INVOKABLE QString text() const;
    Q_INVOKABLE void copy() const;
    Q_INVOKABLE void clear();

    static Entry serverEntry(const QString &line, bool *skip);
    static Entry appLogEntry(const QString &line);

signals:
    void filterChanged();
    void countsChanged();

private:
    void add(Entry entry);
    bool accepts(const Entry &entry) const;
    void refilter();

    int m_limit;
    QList<Entry> m_entries;
    QList<qsizetype> m_visible; // indexes into m_entries
    QList<Entry> m_pending;
    QTimer m_batch;
    QString m_source = QStringLiteral("all");
    bool m_problemsOnly = false;
    QString m_search;
    int m_problems = 0;
    bool m_appOffsetKnown = false;
    int m_appOffset = 0;
    QString shownTime(const Entry &entry) const;
};
