#include "model/MediaItemModel.h"

#include "core/PathUtils.h"

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
        m_pathIndex.insert(item.filePath, m_items.size());
        m_items.append(item);
    }
    endInsertRows();
    emit countChanged();
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
