#pragma once

#include <flostudio/core/project.h>

#include <QAbstractListModel>
#include <QSet>

// The sidebar's project list (D7: the state lives in C++, QML lays it out).
class ProjectsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        MissingRole,
        TypeRole,    // site, siteAdmin, siteApi, siteAdminApi, admin, apiAdmin, api
        RunningRole, // its server is running (stage 4)
    };

    explicit ProjectsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_projects.size()); }
    void setProjects(const QList<FloStudio::Core::ProjectSummary> &projects);
    const QList<FloStudio::Core::ProjectSummary> &projects() const { return m_projects; }
    Q_INVOKABLE int indexOf(const QString &path) const;
    void setRunning(const QSet<QString> &paths);

    static QString typeKey(FloStudio::Core::ProjectType type);

signals:
    void countChanged();

private:
    QList<FloStudio::Core::ProjectSummary> m_projects;
    QSet<QString> m_running;
};
