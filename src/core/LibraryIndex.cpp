#include "core/LibraryIndex.h"

#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>

Q_LOGGING_CATEGORY(lcIndex, "lk.scan.index")

namespace {

// Bump the version whenever the layout below changes; older files are ignored.
constexpr quint32 kMagic = 0x4C4B4958; // "LKIX"
constexpr quint32 kVersion = 2;

void writeTags(QDataStream &stream, const QHash<QString, int> &tags)
{
    stream << static_cast<quint32>(tags.size());
    for (auto it = tags.cbegin(); it != tags.cend(); ++it)
        stream << it.key() << static_cast<qint32>(it.value());
}

QHash<QString, int> readTags(QDataStream &stream)
{
    QHash<QString, int> tags;
    quint32 count = 0;
    stream >> count;
    tags.reserve(static_cast<int>(count));
    for (quint32 i = 0; i < count; ++i) {
        QString key;
        qint32 value = 0;
        stream >> key >> value;
        if (stream.status() != QDataStream::Ok)
            break;
        if (!key.isEmpty())
            tags.insert(key, value);
    }
    return tags;
}

void writeItem(QDataStream &stream, const MediaItem &item)
{
    stream << item.filePath << item.fileName << item.title << item.folderPath << item.relativeFolder
           << item.posterPath << item.nfoPath << item.fanartPath << item.subtitles << item.stillImages
           << item.tags << item.genres << item.actors << item.plot << item.year << item.duration
           << item.fileSize << item.lastModified;
}

MediaItem readItem(QDataStream &stream)
{
    MediaItem item;
    stream >> item.filePath >> item.fileName >> item.title >> item.folderPath >> item.relativeFolder
        >> item.posterPath >> item.nfoPath >> item.fanartPath >> item.subtitles >> item.stillImages
        >> item.tags >> item.genres >> item.actors >> item.plot >> item.year >> item.duration
        >> item.fileSize >> item.lastModified;
    return item;
}

void writeFolder(QDataStream &stream, const LibraryFolderSnapshot &snapshot)
{
    const FolderNode &node = snapshot.node;
    stream << node.path << node.name << node.parentPath << node.posterPath << node.directVideoCount
           << node.totalVideoCount << node.depth << node.childPaths;
    writeTags(stream, node.tagCounts);
    writeTags(stream, snapshot.directTagCounts);
    stream << snapshot.modifiedMs;

    stream << static_cast<quint32>(snapshot.items.size());
    for (const MediaItem &item : snapshot.items)
        writeItem(stream, item);
}

bool readFolder(QDataStream &stream, LibraryFolderSnapshot *snapshot)
{
    FolderNode node;
    stream >> node.path >> node.name >> node.parentPath >> node.posterPath >> node.directVideoCount
        >> node.totalVideoCount >> node.depth >> node.childPaths;
    if (stream.status() != QDataStream::Ok)
        return false;

    node.tagCounts = readTags(stream);
    snapshot->directTagCounts = readTags(stream);
    stream >> snapshot->modifiedMs;

    quint32 itemCount = 0;
    stream >> itemCount;
    if (stream.status() != QDataStream::Ok)
        return false;

    snapshot->items.reserve(static_cast<int>(itemCount));
    for (quint32 i = 0; i < itemCount; ++i) {
        MediaItem item = readItem(stream);
        if (stream.status() != QDataStream::Ok)
            return false;
        snapshot->items.append(item);
    }

    snapshot->node = node;
    return true;
}

} // namespace

bool LibraryIndex::load(const QString &filePath, const QString &rootPath, LibraryIndex *out)
{
    if (!out)
        return false;
    out->clear();

    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return false;

    QDataStream stream(&file);
    quint32 magic = 0;
    quint32 version = 0;
    QString storedRoot;
    stream >> magic >> version >> storedRoot;
    if (stream.status() != QDataStream::Ok || magic != kMagic || version != kVersion) {
        qCInfo(lcIndex) << "ignoring index with a stale format:" << filePath;
        return false;
    }
    if (storedRoot.compare(rootPath, Qt::CaseInsensitive) != 0) {
        qCInfo(lcIndex) << "ignoring index for another library root:" << storedRoot;
        return false;
    }

    quint32 folderCount = 0;
    stream >> folderCount;
    if (stream.status() != QDataStream::Ok)
        return false;

    QHash<QString, LibraryFolderSnapshot> folders;
    folders.reserve(static_cast<int>(folderCount));
    for (quint32 i = 0; i < folderCount; ++i) {
        LibraryFolderSnapshot snapshot;
        if (!readFolder(stream, &snapshot))
            break;
        if (snapshot.node.path.isEmpty())
            continue;
        folders.insert(snapshot.node.path, snapshot);
    }

    // A truncated file is still useful: whatever could be read is a valid
    // snapshot of those folders, the rest is simply re-scanned.
    if (folders.isEmpty())
        return false;

    out->m_root = storedRoot;
    out->m_folders = folders;
    return true;
}

bool LibraryIndex::save(const QString &filePath) const
{
    if (m_folders.isEmpty() || m_root.isEmpty())
        return false;

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcIndex) << "cannot write index" << filePath;
        return false;
    }

    QDataStream stream(&file);
    stream << kMagic << kVersion << m_root << static_cast<quint32>(m_folders.size());
    for (auto it = m_folders.cbegin(); it != m_folders.cend(); ++it)
        writeFolder(stream, it.value());

    if (stream.status() != QDataStream::Ok || !file.commit()) {
        qCWarning(lcIndex) << "failed to persist index" << filePath;
        return false;
    }
    return true;
}
