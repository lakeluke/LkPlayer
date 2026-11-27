#include "model/MediaItemModel.h"

#include <algorithm>

#include "core/PathUtils.h"

namespace {

QString sortKeyFor(const MediaItem &item, const QString &field)
{
    if (field == QLatin1String("title"))
        return item.title.isEmpty() ? item.fileName : item.title;
    if (field == QLatin1String("name"))
        return item.fileName;
    if (field == QLatin1String("path"))
        return item.filePath;
    return item.fileName;
}

int compareItems(const MediaItem &a, const MediaItem &b, const QString &field)
{
    if (field == QLatin1String("scan")) {
        if (a.scanOrder != b.scanOrder)
            return a.scanOrder < b.scanOrder ? -1 : 1;
        return 0;
    }
    if (field == QLatin1String("duration")) {
        if (a.duration != b.duration)
            return a.duration < b.duration ? -1 : 1;
        return 0;
    }
    if (field == QLatin1String("modified")) {
        if (a.lastModified != b.lastModified)
            return a.lastModified < b.lastModified ? -1 : 1;
        return 0;
    }
    if (field == QLatin1String("release")) {
        // Missing year sorts as "0" (oldest) so it lands last in the natural
        // newest-first order and first in oldest-first.
        const QString ka = a.year.trimmed().isEmpty() ? QStringLiteral("0") : a.year.trimmed();
        const QString kb = b.year.trimmed().isEmpty() ? QStringLiteral("0") : b.year.trimmed();
        return QString::compare(ka, kb);
    }
    return QString::localeAwareCompare(sortKeyFor(a, field), sortKeyFor(b, field));
}

} // namespace

MediaItemModel::MediaItemModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MediaItemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant MediaItemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return QVariant();

    const MediaItem &item = m_items.at(index.row());

    switch (role) {
    case FilePathRole:
        return item.filePath;
    case FileNameRole:
        return item.fileName;
    case TitleRole:
        return item.title.isEmpty() ? item.fileName : item.title;
    case FolderPathRole:
        return item.folderPath;
    case RelativeFolderRole:
        return item.relativeFolder;
    case PosterPathRole:
        return item.posterPath;
    case NfoPathRole:
        return item.nfoPath;
    case FanartPathRole:
        return item.fanartPath;
    case SubtitlesRole:
        return item.subtitles;
    case StillImagesRole:
        return item.stillImages;
    case TagsRole:
        return item.tags;
    case GenresRole:
        return item.genres;
    case ActorsRole:
        return item.actors;
    case PlotRole:
        return item.plot;
    case YearRole:
        return item.year;
    case PremieredRole:
        return item.premiered;
    case ModifiedRole:
        return item.lastModified;
    case DurationRole:
        return item.duration;
    case DurationTextRole:
        return item.duration > 0.0 ? PathUtils::formatDuration(item.duration) : QString();
    case SizeTextRole:
        return PathUtils::formatSize(item.fileSize);
    case HasPosterRole:
        return !item.posterPath.isEmpty();
    case HasSubtitlesRole:
        return !item.subtitles.isEmpty();
    case StillCountRole:
        return item.stillImages.size();
    case CoverUrlRole:
        if (!item.posterPath.isEmpty())
            return QUrl::fromLocalFile(item.posterPath);
        return QUrl(QStringLiteral("image://thumb/%1").arg(PathUtils::encodeId(item.filePath)));
    default:
        break;
    }
    return QVariant();
}

QHash<int, QByteArray> MediaItemModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { FilePathRole, "filePath" },
        { FileNameRole, "fileName" },
        { TitleRole, "title" },
        { FolderPathRole, "folderPath" },
        { RelativeFolderRole, "relativeFolder" },
        { PosterPathRole, "posterPath" },
        { NfoPathRole, "nfoPath" },
        { FanartPathRole, "fanartPath" },
        { SubtitlesRole, "subtitles" },
        { StillImagesRole, "stillImages" },
        { TagsRole, "tags" },
        { GenresRole, "genres" },
        { ActorsRole, "actors" },
        { PlotRole, "plot" },
        { YearRole, "year" },
        { PremieredRole, "premiered" },
        { ModifiedRole, "lastModified" },
        { DurationRole, "durationSeconds" },
        { DurationTextRole, "durationText" },
        { SizeTextRole, "sizeText" },
        { CoverUrlRole, "coverUrl" },
        { HasPosterRole, "hasPoster" },
        { HasSubtitlesRole, "hasSubtitles" },
        { StillCountRole, "stillCount" },
    };
    return roles;
}

void MediaItemModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    m_pathIndex.clear();
    endResetModel();
    emit countChanged();
}

void MediaItemModel::addItems(const MediaItemList &items)
{
    if (items.isEmpty())
        return;

    const int first = m_items.size();
    beginInsertRows(QModelIndex(), first, first + items.size() - 1);
    for (const MediaItem &item : items) {
        MediaItem copy = item;
        copy.scanOrder = m_items.size();
        m_pathIndex.insert(copy.filePath, m_items.size());
        m_items.append(copy);
    }
    endInsertRows();
    emit countChanged();
}

void MediaItemModel::sortBy(const QString &field, bool ascending)
{
    if (m_items.size() < 2)
        return;

    MediaItemList ordered = m_items;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [field, ascending](const MediaItem &a, const MediaItem &b) {
                         const int cmp = compareItems(a, b, field);
                         return ascending ? cmp < 0 : cmp > 0;
                     });

    beginResetModel();
    m_items = ordered;
    endResetModel();

    m_pathIndex.clear();
    m_pathIndex.reserve(m_items.size());
    for (int row = 0; row < m_items.size(); ++row)
        m_pathIndex.insert(m_items.at(row).filePath, row);

    emit countChanged();
}

void MediaItemModel::replaceFolder(const QString &folderPath, const MediaItemList &items)
{
    if (folderPath.isEmpty())
        return;

    bool removed = false;
    for (int row = m_items.size() - 1; row >= 0; --row) {
        if (m_items.at(row).folderPath != folderPath)
            continue;
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        endRemoveRows();
        removed = true;
    }

    if (removed) {
        m_pathIndex.clear();
        m_pathIndex.reserve(m_items.size());
        for (int row = 0; row < m_items.size(); ++row)
            m_pathIndex.insert(m_items.at(row).filePath, row);
        emit countChanged();
    }

    if (!items.isEmpty())
        addItems(items);
}

const MediaItem *MediaItemModel::itemAt(int row) const
{
    if (row < 0 || row >= m_items.size())
        return nullptr;
    return &m_items.at(row);
}

int MediaItemModel::indexOfPath(const QString &filePath) const
{
    return m_pathIndex.value(filePath, -1);
}

QVariantMap MediaItemModel::item(int row) const
{
    const MediaItem *entry = itemAt(row);
    if (!entry)
        return QVariantMap();
    return toMap(*entry);
}

QVariantMap MediaItemModel::toMap(const MediaItem &item) const
{
    QVariantMap map;
    map.insert(QStringLiteral("filePath"), item.filePath);
    map.insert(QStringLiteral("fileName"), item.fileName);
    map.insert(QStringLiteral("title"), item.title.isEmpty() ? item.fileName : item.title);
    map.insert(QStringLiteral("folderPath"), item.folderPath);
    map.insert(QStringLiteral("relativeFolder"), item.relativeFolder);
    map.insert(QStringLiteral("posterPath"), item.posterPath);
    map.insert(QStringLiteral("fanartPath"), item.fanartPath);
    map.insert(QStringLiteral("nfoPath"), item.nfoPath);
    map.insert(QStringLiteral("plot"), item.plot);
    map.insert(QStringLiteral("year"), item.year);
    map.insert(QStringLiteral("premiered"), item.premiered);
    map.insert(QStringLiteral("tags"), item.tags);
    map.insert(QStringLiteral("genres"), item.genres);
    map.insert(QStringLiteral("actors"), item.actors);
    map.insert(QStringLiteral("subtitles"), item.subtitles);
    map.insert(QStringLiteral("stillImages"), item.stillImages);
    map.insert(QStringLiteral("duration"), item.duration);
    map.insert(QStringLiteral("durationText"), item.duration > 0.0 ? PathUtils::formatDuration(item.duration) : QString());
    map.insert(QStringLiteral("sizeText"), PathUtils::formatSize(item.fileSize));
    map.insert(QStringLiteral("coverUrl"),
               item.posterPath.isEmpty()
                   ? QUrl(QStringLiteral("image://thumb/%1").arg(PathUtils::encodeId(item.filePath)))
                   : QUrl::fromLocalFile(item.posterPath));
    return map;
}
