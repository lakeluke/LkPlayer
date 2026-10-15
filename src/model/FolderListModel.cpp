#include "model/FolderListModel.h"

#include <QDir>
#include <QUrl>

#include "core/PathUtils.h"

FolderListModel::FolderListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FolderListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_children.size();
}

QVariant FolderListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_children.size())
        return QVariant();

    const QString path = m_children.at(index.row());
    const FolderNode node = m_nodes.value(path);

    switch (role) {
    case NameRole:
        return node.name;
    case PathRole:
        return path;
    case HasCoverRole:
        return !node.posterPath.isEmpty() || m_coverHints.contains(path);
    case CoverUrlRole: {
        // Folder artwork wins, otherwise the caller supplied a ready made
        // cover URL (usually the thumbnail of the first video inside).
        if (!node.posterPath.isEmpty())
            return QUrl::fromLocalFile(node.posterPath);
        const QString hint = m_coverHints.value(path);
        if (!hint.isEmpty())
            return QUrl(hint);
        // An explicitly empty url keeps QML happy (undefined would fail to
        // convert into the delegate's `url` property).
        return QUrl();
    }
    case VideoCountRole:
        return node.totalVideoCount;
    case RelativePathRole: {
        if (m_rootPath.isEmpty())
            return path;
        const QString rel = QDir(m_rootPath).relativeFilePath(path);
        return rel == QLatin1String(".") ? QString() : rel;
    }
    case DepthRole:
        return node.depth;
    default:
        break;
    }
    return QVariant();
}

QHash<int, QByteArray> FolderListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { NameRole, "name" },
        { PathRole, "path" },
        { CoverUrlRole, "coverUrl" },
        { HasCoverRole, "hasCover" },
        { VideoCountRole, "videoCount" },
        { RelativePathRole, "relativePath" },
        { DepthRole, "depth" },
    };
    return roles;
}

void FolderListModel::setFolders(const FolderNodeList &nodes)
{
    m_nodes.clear();
    m_nodes.reserve(nodes.size());
    for (const FolderNode &node : nodes)
        m_nodes.insert(node.path, node);
    rebuild();
}

void FolderListModel::setCoverHints(const QHash<QString, QString> &folderToVideo)
{
    m_coverHints = folderToVideo;
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), { CoverUrlRole, HasCoverRole });
}

void FolderListModel::setRootPath(const QString &rootPath)
{
    m_rootPath = rootPath;
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), { RelativePathRole });
}

void FolderListModel::setCurrentFolder(const QString &folder)
{
    const QString cleaned = QDir::cleanPath(folder);
    if (cleaned == m_currentFolder)
        return;
    m_currentFolder = cleaned;
    emit currentFolderChanged();
    rebuild();
}

void FolderListModel::rebuild()
{
    beginResetModel();
    m_children = m_nodes.value(m_currentFolder).childPaths;
    m_children.sort(Qt::CaseInsensitive);
    endResetModel();
    emit countChanged();
}

QString FolderListModel::pathAt(int row) const
{
    if (row < 0 || row >= m_children.size())
        return QString();
    return m_children.at(row);
}
