#include "core/LibraryScanner.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QtConcurrent/QtConcurrentRun>

#include "core/NfoParser.h"
#include "core/PathUtils.h"

Q_LOGGING_CATEGORY(lcScan, "lk.scan")

namespace {

constexpr int kBatchSize = 48;
constexpr qint64 kBatchIntervalMs = 150;
constexpr qint64 kProgressIntervalMs = 120;
constexpr int kMaxDepth = 24;

QStringList normalizedTags(const QStringList &input)
{
    QStringList result;
    for (const QString &entry : input) {
        const QString value = entry.trimmed();
        if (value.isEmpty())
            continue;
        bool duplicate = false;
        for (const QString &existing : result) {
            if (existing.compare(value, Qt::CaseInsensitive) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            result.append(value);
    }
    result.sort(Qt::CaseInsensitive);
    return result;
}

/** Resolves an nfo-relative image reference (nfo files may point to a sibling). */
QString resolveNfoImage(const QString &folderPath, const QString &reference)
{
    if (reference.isEmpty())
        return QString();

    const QString trimmed = reference.trimmed();
    if (trimmed.startsWith(QLatin1String("http://")) || trimmed.startsWith(QLatin1String("https://")))
        return QString();

    QFileInfo info(trimmed);
    if (!info.isAbsolute())
        info = QFileInfo(QDir(folderPath).absoluteFilePath(trimmed));

    if (info.exists() && info.isFile())
        return info.absoluteFilePath();
    return QString();
}

} // namespace

LibraryScanner::LibraryScanner(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<MediaItem>("MediaItem");
    qRegisterMetaType<MediaItemList>("MediaItemList");
    qRegisterMetaType<FolderNode>("FolderNode");
    qRegisterMetaType<FolderNodeList>("FolderNodeList");

    connect(&m_watcher, &QFutureWatcher<void>::finished, this, [this]() {
        const bool cancelled = m_cancelFlag.loadRelaxed() != 0;
        m_running = false;
        emit runningChanged(false);
        emit scanFinished(m_videoCount, cancelled);
    });
}

LibraryScanner::~LibraryScanner()
{
    m_cancelFlag.storeRelaxed(1);
    if (m_future.isRunning())
        m_future.waitForFinished();
}

void LibraryScanner::setRootPath(const QString &path)
{
    const QString cleaned = QDir::cleanPath(path);
    if (cleaned == m_root)
        return;
    m_root = cleaned;
    emit rootPathChanged(m_root);
}

void LibraryScanner::start()
{
    if (m_running) {
        qCInfo(lcScan) << "scan already running, request ignored";
        return;
    }

    if (m_root.isEmpty()) {
        emit errorOccurred(QStringLiteral("尚未设置媒体库根目录"));
        return;
    }

    const QFileInfo rootInfo(m_root);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        emit errorOccurred(QStringLiteral("媒体库目录不存在：%1").arg(m_root));
        return;
    }

    m_rootAbsolute = rootInfo.absoluteFilePath();
    m_cancelFlag.storeRelaxed(0);
    m_runningFlag.storeRelaxed(1);
    m_scannedFiles = 0;
    m_videoCount = 0;
    m_lastEmitMs = QDateTime::currentMSecsSinceEpoch();
    m_lastProgressMs = m_lastEmitMs;

    // Worker state is created inside the worker thread to avoid sharing.
    m_running = true;
    emit runningChanged(true);
    emit scanStarted();

    qCInfo(lcScan) << "scan started:" << m_rootAbsolute;
    m_future = QtConcurrent::run([this]() { runScan(); });
    m_watcher.setFuture(m_future);
}

void LibraryScanner::cancel()
{
    if (!m_running)
        return;
    qCInfo(lcScan) << "scan cancelled by user";
    m_cancelFlag.storeRelaxed(1);
}

void LibraryScanner::runScan()
{
    m_pendingItems.clear();
    m_folders.clear();
    m_scannedFiles = 0;
    m_videoCount = 0;

    {
        FolderNode node;
        node.path = m_rootAbsolute;
        const QString dirName = QDir(m_rootAbsolute).dirName();
        node.name = dirName.isEmpty() ? m_rootAbsolute : dirName;
        node.depth = 0;
        m_folders.insert(node.path, node);
    }

    scanDirectory(m_rootAbsolute, 0);
    flushItems(true);

    FolderNodeList list;
    list.reserve(m_folders.size());
    for (auto it = m_folders.cbegin(); it != m_folders.constEnd(); ++it)
        list.append(it.value());

    emit structureReady(list);
    emit progressChanged(m_scannedFiles, m_videoCount, m_rootAbsolute);
    qCInfo(lcScan) << "scan finished:" << m_videoCount << "videos," << m_scannedFiles << "files," << list.size()
                   << "folders";
}

void LibraryScanner::scanDirectory(const QString &dirPath, int depth)
{
    if (m_cancelFlag.loadRelaxed() || depth > kMaxDepth)
        return;

    PathUtils::DirIndex index;
    index.build(dirPath);
    if (!index.isValid())
        return;

    const QDir dir(dirPath);
    const QString absPath = dir.absolutePath();
    const QString folderName = dir.dirName();

    QStringList childDirs;
    childDirs.reserve(index.subDirs().size());
    for (const QString &name : index.subDirs()) {
        if (PathUtils::isIgnoredDirectory(name))
            continue;
        childDirs.append(dir.absoluteFilePath(name));
    }
    childDirs.sort(Qt::CaseInsensitive);

    const QString folderPoster = PathUtils::findFolderPoster(index, folderName);

    QVector<MediaItem> ownItems;
    ownItems.reserve(8);

    for (const QString &fileName : index.files()) {
        if (m_cancelFlag.loadRelaxed())
            return;

        ++m_scannedFiles;
        if (!PathUtils::isVideoFile(fileName))
            continue;

        const QFileInfo info(dir.absoluteFilePath(fileName));
        const QString videoBase = PathUtils::baseName(fileName);

        MediaItem item;
        item.filePath = info.absoluteFilePath();
        item.fileName = info.fileName();
        item.folderPath = absPath;
        item.relativeFolder = QDir(m_rootAbsolute).relativeFilePath(absPath);
        if (item.relativeFolder == QLatin1String("."))
            item.relativeFolder.clear();
        item.fileSize = info.size();
        item.lastModified = info.lastModified().toMSecsSinceEpoch();
        item.title = videoBase;

        // A dedicated poster wins; otherwise the folder artwork is shared by
        // every video stored in that folder.
        item.posterPath = PathUtils::findPoster(index, videoBase);
        if (item.posterPath.isEmpty())
            item.posterPath = folderPoster;
        item.fanartPath = PathUtils::findFanart(index, videoBase, item.posterPath);
        item.stillImages = PathUtils::findStillImages(index, videoBase, item.posterPath);
        item.subtitles = PathUtils::findSubtitles(index, videoBase);
        item.nfoPath = PathUtils::findNfo(index, videoBase, folderName);

        if (!item.nfoPath.isEmpty()) {
            QString parseError;
            const NfoData nfo = NfoParser::parseFile(item.nfoPath, &parseError);
            if (!parseError.isEmpty())
                qCDebug(lcScan) << "nfo problem:" << item.nfoPath << parseError;

            if (!nfo.title.isEmpty())
                item.title = nfo.title;
            item.plot = nfo.plot.isEmpty() ? nfo.outline : nfo.plot;
            item.genres = normalizedTags(nfo.genres);
            item.actors = nfo.actors;
            item.year = nfo.year;
            if (item.posterPath.isEmpty())
                item.posterPath = resolveNfoImage(absPath, nfo.posterThumb);
            if (item.fanartPath.isEmpty())
                item.fanartPath = resolveNfoImage(absPath, nfo.fanartThumb);
            if (nfo.runtimeMinutes > 0)
                item.duration = nfo.runtimeMinutes * 60.0;
            item.tags = NfoParser::normalizedTags(nfo);
        }

        if (item.tags.isEmpty())
            item.tags = item.genres;
        else
            item.tags = normalizedTags(item.tags);

        ownItems.append(item);
        ++m_videoCount;
    }

    // Commit the folder node (never keep a reference across hash mutations).
    {
        FolderNode &node = m_folders[absPath];
        node.path = absPath;
        node.name = folderName.isEmpty() ? absPath : folderName;
        node.depth = depth;
        node.childPaths = childDirs;
        node.directVideoCount = ownItems.size();
        node.posterPath = folderPoster;

        if (!ownItems.isEmpty()) {
            QHash<QString, int> localCounts;
            for (const MediaItem &item : ownItems) {
                for (const QString &tag : item.tags)
                    localCounts[tag] += 1;
            }
            node.tagCounts = localCounts;
        }
    }

    m_pendingItems += ownItems;
    flushItems(false);
    emitProgress(false);

    for (const QString &child : childDirs) {
        if (m_cancelFlag.loadRelaxed())
            break;
        scanDirectory(child, depth + 1);
    }

    // Roll the video counts up now that every descendant has been visited.
    int total = m_folders.value(absPath).directVideoCount;
    QHash<QString, int> mergedTags = m_folders.value(absPath).tagCounts;
    for (const QString &child : childDirs) {
        const FolderNode childNode = m_folders.value(child);
        total += childNode.totalVideoCount;
        for (auto it = childNode.tagCounts.cbegin(); it != childNode.tagCounts.constEnd(); ++it)
            mergedTags[it.key()] = mergedTags.value(it.key()) + it.value();
    }

    FolderNode &node = m_folders[absPath];
    node.totalVideoCount = total;
    node.tagCounts = mergedTags;
}

void LibraryScanner::flushItems(bool force)
{
    if (m_pendingItems.isEmpty())
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!force && m_pendingItems.size() < kBatchSize && (now - m_lastEmitMs) < kBatchIntervalMs)
        return;

    MediaItemList batch;
    batch.swap(m_pendingItems);
    m_lastEmitMs = now;
    emit itemsReady(batch);
}

void LibraryScanner::emitProgress(bool force)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!force && (now - m_lastProgressMs) < kProgressIntervalMs)
        return;
    m_lastProgressMs = now;
    emit progressChanged(m_scannedFiles, m_videoCount, m_rootAbsolute);
}
