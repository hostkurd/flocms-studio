#pragma once

#include <QLockFile>
#include <QObject>
#include <QString>

class QLocalServer;

// One Studio per user: the first copy holds a lock file and listens on a
// local socket only this user can open; a second copy asks it to show
// its window (`show`), or to open a folder (`open <path>`, from --open:
// FloServer opens a project in Studio, D14) and, once it has answered,
// quits. Studio always starts when
// nothing answers: a lock left by a Studio that crashed (here or on
// another computer sharing the home folder), a settings folder that can't
// be written, a Studio that is closing. Two copies at worst, never none.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    // `folder`: Studio's settings folder (the lock, and the socket's name)
    explicit SingleInstance(const QString &folder, QObject *parent = nullptr);
    ~SingleInstance() override;
    SingleInstance(const SingleInstance &) = delete;
    SingleInstance &operator=(const SingleInstance &) = delete;

    // False only when a running Studio answered that it shows itself:
    // then this copy should quit. True: run (alone, or when nothing
    // answered).
    // With `openFolder`, a running Studio is asked to open it instead;
    // one that can't be handed over (canHandOver) is not sent: that Studio
    // is only asked to show itself.
    bool claim(const QString &openFolder = {});
    // Whether `open <folder>` can be sent: one line, an absolute path,
    // the message at most 8 KB
    static bool canHandOver(const QString &folder);
    // Closing: no longer answers, so a new copy started now runs
    void release();
    // The socket's name: from a random secret in the settings folder (only
    // this user can read it, so another user can't take the name first),
    // and on Windows the login session (each session its own)
    QString socketName() const { return m_name; }

signals:
    // A second copy was started: show the window
    void activateRequested();
    // A second copy was started with --open: open this folder (an absolute
    // path; Studio checks it is a FloCMS project), then show the window
    void openRequested(const QString &folder);

private:
    enum class Answer { Shown, NoAnswer };
    Answer askRunningCopy(const QString &openFolder);
    void listen();

    QString m_folder;
    QString m_name;
    QLockFile m_lock;
    QLocalServer *m_server = nullptr; // a child of this
};
