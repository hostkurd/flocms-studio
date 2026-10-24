#pragma once

#include <flostudio/core/generators.h>
#include <flostudio/core/settings.h>

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <memory>

class QProcess;

// One project's Generate tab (stage 7, board 3): a form per FloCMS
// generator, its preview (D5: the real generator in a temporary copy of
// the project; libs/core preview.h) and Create, which runs the same
// command in the project. Belongs to Studio, which outlives it, so the
// settings pointer stays valid.
class ProjectGenerate : public QObject
{
    Q_OBJECT
    // admin-resource, admin-page, block, widget, controller, migration
    Q_PROPERTY(QString generator READ generator NOTIFY formChanged)
    // The form's fields (generators.h GeneratorForm), by name
    Q_PROPERTY(QVariantMap form READ form NOTIFY formChanged)
    // {field: code} for the fields touched so far
    Q_PROPERTY(QVariantMap problems READ problems NOTIFY formChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY formChanged)
    // The command as it will run
    Q_PROPERTY(QString command READ command NOTIFY formChanged)
    Q_PROPERTY(bool hasAdmin READ hasAdmin NOTIFY formChanged)
    // {state (idle, copying, running, done, failed), shown, files: [{path,
    //  kind (added, changed, removed), binary, tooBig, lines: [{kind, text}]}],
    //  lines (what the generator printed), error, detail, message, current
    //  (false once the form changed since), outside (project files that
    //  changed while it ran), notRemoved (the copy could not be removed)}
    Q_PROPERTY(QVariantMap preview READ preview NOTIFY previewChanged)
    // Create: {state (running, done, failed), shown, lines, error, detail, message}
    Q_PROPERTY(QVariantMap created READ created NOTIFY createdChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    ProjectGenerate(QString projectDir, const FloStudio::Core::Settings *settings, bool hasAdmin, QObject *parent = nullptr);
    ~ProjectGenerate() override;
    ProjectGenerate(const ProjectGenerate &) = delete;
    ProjectGenerate &operator=(const ProjectGenerate &) = delete;

    QString generator() const;
    QVariantMap form() const;
    QVariantMap problems() const;
    bool ready() const;
    QString command() const;
    bool hasAdmin() const { return m_hasAdmin; }
    QVariantMap preview() const;
    QVariantMap created() const { return m_created; }
    bool busy() const;
    // Also a copy still being made or removed (tests wait for this)
    bool working() const { return busy() || m_copying > 0; }
    void setHasAdmin(bool hasAdmin);

    // Picks a generator; its form starts empty.
    Q_INVOKABLE void choose(const QString &generator);
    Q_INVOKABLE void set(const QString &field, const QVariant &value);
    Q_INVOKABLE void touch(const QString &field);
    // The real generator in a copy: what it would write.
    Q_INVOKABLE void runPreview();
    // The command in the project.
    Q_INVOKABLE void create();

    // A demo: the form and its preview from the demo file.
    void showDemo(const QVariantMap &demo);

    // How long a generator may take (tests make it short).
    static void setTimeoutSeconds(int seconds);

signals:
    void formChanged();
    void previewChanged();
    void createdChanged();
    void busyChanged();
    // Create wrote into the project: the Database tab and Doctor read again
    void changedProject();

private:
    QStringList arguments() const;
    void previewCopied(int run, const QString &copy, const QString &error, const QString &detail);
    void previewRan(int run, const QString &copy, int exitCode, bool crashed);
    void previewFailed(const QString &error, const QString &detail);
    void stopPreview();
    bool demo() const { return m_settings == nullptr; }

    QString m_projectDir;
    const FloStudio::Core::Settings *m_settings; // Studio's; null for a demo
    bool m_hasAdmin;
    FloStudio::Core::Generator m_generator;
    FloStudio::Core::GeneratorForm m_form;
    QStringList m_touched;

    QVariantMap m_preview{{QStringLiteral("state"), QStringLiteral("idle")}};
    QStringList m_previewArguments; // what the preview ran
    int m_previewRun = 0;           // a newer preview makes an older one's results void
    std::shared_ptr<std::atomic<bool>> m_cancel = std::make_shared<std::atomic<bool>>(false);
    QString m_copy;                 // the copy while its generator runs
    QStringList m_copied;           // its files as they were made
    QStringList m_projectBefore;    // the project's then
    bool m_previewTimedOut = false;
    QProcess *m_previewProcess = nullptr; // a child of this while it runs
    QStringList m_previewOut;
    QStringList m_previewErr;
    int m_copying = 0; // copies and removals still on a worker

    QVariantMap m_created;
    QProcess *m_createProcess = nullptr; // the same
    QStringList m_createOut;
    QStringList m_createErr;
};
