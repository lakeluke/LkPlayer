#include "model/MediaFilterProxyModel.h"

#include <QDir>

#include <algorithm>

#include "model/MediaItemModel.h"

namespace {

bool samePath(const QString &a, const QString &b)
{
    if (a.isEmpty() || b.isEmpty())
        return false;
    return QDir::cleanPath(a).compare(QDir::cleanPath(b), Qt::CaseInsensitive) == 0;
}

} // namespace

MediaFilterProxyModel::MediaFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortRole(MediaItemModel::TitleRole);
    sort(0, Qt::AscendingOrder);
}

MediaItemModel *MediaFilterProxyModel::sourceItemModel() const
{
    return qobject_cast<MediaItemModel *>(sourceModel());
}

void MediaFilterProxyModel::setViewMode(const QString &mode)
{
    const QString normalised = (mode == QLatin1String("drill")) ? QStringLiteral("drill") : QStringLiteral("flat");
    if (normalised == m_viewMode)
        return;
    m_viewMode = normalised;
    emit viewModeChanged();
    refreshFilter();
}

void MediaFilterProxyModel::setCurrentFolder(const QString &folder)
{
    const QString cleaned = QDir::cleanPath(folder);
    if (cleaned == m_currentFolder)
        return;
    m_currentFolder = cleaned;
    emit currentFolderChanged();
    refreshFilter();
}

void MediaFilterProxyModel::setSelectedTags(const QStringList &tags)
{
    if (tags == m_selectedTags)
        return;
    m_selectedTags = tags;
    std::sort(m_selectedTags.begin(), m_selectedTags.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    emit selectedTagsChanged();
    refreshFilter();
}

void MediaFilterProxyModel::toggleTag(const QString &tag)
{
    QStringList tags = m_selectedTags;
    int found = -1;
    for (int i = 0; i < tags.size(); ++i) {
        if (tags.at(i).compare(tag, Qt::CaseInsensitive) == 0) {
            found = i;
            break;
        }
    }
    if (found >= 0)
        tags.removeAt(found);
    else
        tags.append(tag);
    setSelectedTags(tags);
}

void MediaFilterProxyModel::clearTags()
{
    setSelectedTags(QStringList());
}

void MediaFilterProxyModel::setSearchText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed == m_searchText)
        return;
    m_searchText = trimmed;
    emit searchTextChanged();
    refreshFilter();
}

void MediaFilterProxyModel::refreshFilter()
{
    m_availableTagsDirty = true;
    invalidateFilter();
    emit countChanged();
    emit availableTagsChanged();
}

bool MediaFilterProxyModel::rowInScope(int sourceRow) const
{
    if (m_viewMode != QLatin1String("drill"))
        return true;
    MediaItemModel *model = sourceItemModel();
    if (!model)
        return true;
    const MediaItem *item = model->itemAt(sourceRow);
    if (!item)
        return false;
    return samePath(item->folderPath, m_currentFolder);
}

bool MediaFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent);
    MediaItemModel *model = sourceItemModel();
    if (!model)
        return false;

    const MediaItem *item = model->itemAt(sourceRow);
    if (!item)
        return false;

    if (!rowInScope(sourceRow))
        return false;

    if (!m_selectedTags.isEmpty()) {
        bool matched = false;
        for (const QString &tag : item->tags) {
            for (const QString &selected : m_selectedTags) {
                if (tag.compare(selected, Qt::CaseInsensitive) == 0) {
                    matched = true;
                    break;
                }
            }
            if (matched)
                break;
        }
        if (!matched)
            return false;
    }

    if (!m_searchText.isEmpty()) {
        const bool hit = item->title.contains(m_searchText, Qt::CaseInsensitive)
                         || item->fileName.contains(m_searchText, Qt::CaseInsensitive)
                         || item->relativeFolder.contains(m_searchText, Qt::CaseInsensitive);
        if (!hit)
            return false;
    }

    return true;
}

QStringList MediaFilterProxyModel::availableTags() const
{
    if (!m_availableTagsDirty)
        return m_availableTags;

    MediaItemModel *model = sourceItemModel();
    if (!model) {
        m_availableTags.clear();
        m_availableTagsDirty = false;
        return m_availableTags;
    }

    QStringList tags;
    const int rows = model->rowCount();
    for (int row = 0; row < rows; ++row) {
        if (!rowInScope(row))
            continue;
        const MediaItem *item = model->itemAt(row);
        if (!item)
            continue;
        for (const QString &tag : item->tags) {
            bool duplicate = false;
            for (const QString &known : tags) {
                if (known.compare(tag, Qt::CaseInsensitive) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
                tags.append(tag);
        }
    }
    tags.sort(Qt::CaseInsensitive);
    m_availableTags = tags;
    m_availableTagsDirty = false;
    return m_availableTags;
}

QString MediaFilterProxyModel::filePathAt(int row) const
{
    if (row < 0 || row >= rowCount())
        return QString();
    return data(index(row, 0), MediaItemModel::FilePathRole).toString();
}
