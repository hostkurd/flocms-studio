#include "projectsmodel.h"

using namespace FloStudio::Core;

ProjectsModel::ProjectsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ProjectsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant ProjectsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count())
        return {};
    const ProjectSummary &project = m_projects.at(index.row());
    switch (role) {
    case NameRole:
    case Qt::DisplayRole:
        return project.folderName;
    case PathRole:
        return project.path;
    case MissingRole:
        return project.missing;
    case TypeRole:
        return typeKey(projectType(project));
    default:
        return {};
    }
}

QHash<int, QByteArray> ProjectsModel::roleNames() const
{
    return {{NameRole, "name"}, {PathRole, "path"}, {MissingRole, "missing"}, {TypeRole, "type"}};
}

void ProjectsModel::setProjects(const QList<ProjectSummary> &projects)
{
    if (projects == m_projects)
        return;
    beginResetModel();
    m_projects = projects;
    endResetModel();
    emit countChanged();
}

int ProjectsModel::indexOf(const QString &path) const
{
    for (qsizetype i = 0; i < m_projects.size(); ++i) {
        if (m_projects.at(i).path == path)
            return static_cast<int>(i);
    }
    return -1;
}

QString ProjectsModel::typeKey(ProjectType type)
{
    switch (type) {
    case ProjectType::Site:
        return QStringLiteral("site");
    case ProjectType::SiteAdmin:
        return QStringLiteral("siteAdmin");
    case ProjectType::SiteApi:
        return QStringLiteral("siteApi");
    case ProjectType::SiteAdminApi:
        return QStringLiteral("siteAdminApi");
    case ProjectType::Admin:
        return QStringLiteral("admin");
    case ProjectType::ApiAdmin:
        return QStringLiteral("apiAdmin");
    case ProjectType::Api:
        return QStringLiteral("api");
    }
    return QStringLiteral("site");
}
