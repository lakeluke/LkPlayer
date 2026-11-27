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
constexpr qint64 kStructureIntervalMs = 700;
constexpr int kMaxDepth = 24;

QString modeName(LibraryScanner::Mode mode)
{
    switch (mode) {
    case LibraryScanner::Mode::Snapshot:
        return QStringLiteral("snapshot");
    case LibraryScanner::Mode::Verify:
        return QStringLiteral("verify");
    case LibraryScanner::Mode::Full:
        return QStringLiteral("full");
    case LibraryScanner::Mode::LoadOnly:
        return QStringLiteral("loadonly");
    }
    return QStringLiteral("snapshot");
}

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

/**
 * A directory's own timestamp. It changes whenever a child entry is created,
 * removed or renamed, which is exactly the granularity the incremental scan
 * needs; editing a file's content leaves it untouched.
 */
qint64 directoryModifiedMs(const QString &dirPath)
{
    return QFileInfo(dirPath).lastModified().toMSecsSinceEpoch();
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
    startWithMode(Mode::Snapshot);
}

void LibraryScanner::startWithMode(Mode mode)
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
    m_mode = mode;
    m_cancelFlag.storeRelaxed(0);
    m_scannedFiles = 0;
    m_videoCount = 0;
    m_lastEmitMs = QDateTime::currentMSecsSinceEpoch();
    m_lastProgressMs = m_lastEmitMs;
    m_lastStructureMs = 0;

    // Worker state is created inside the worker thread to avoid sharing.
    m_running = true;
    emit runningChanged(true);
    emit scanStarted();

    qCInfo(lcScan) << "scan started (" << modeName(mode) << "):" << m_rootAbsolute;
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

FolderNodeList LibraryScanner::folderList() const
{
    FolderNodeList list;
    list.reserve(m_folders.size());
    for (auto it = m_folders.cbegin(); it != m_folders.constEnd(); ++it)
        list.append(it.value());
    return list;
}

void LibraryScanner::runScan()
{
    m_pendingItems.clear();
    m_folders.clear();
    m_index.clear();
    m_haveIndex = false;
    m_deltaMode = false;
    m_indexDirty = false;
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

    if (m_mode != Mode::Full && !m_indexFile.isEmpty())
        m_haveIndex = LibraryIndex::load(m_indexFile, m_rootAbsolute, &m_index);
    // Without a cache the model starts empty, so full batches have to be sent.
    m_deltaMode = m_haveIndex && m_mode != Mode::Full;

    // Publish the previous result immediately: the grid is usable within
    // milliseconds even when the verification below has to touch the disk.
    if (m_haveIndex && (m_mode == Mode::Snapshot || m_mode == Mode::LoadOnly)) {
        int videos = 0;
        MediaItemList batch;
        batch.reserve(kBatchSize);
        for (const LibraryFolderSnapshot &folder : m_index.folders()) {
            for (const MediaItem &item : folder.items) {
                batch.append(item);
                ++videos;
                if (batch.size() >= kBatchSize) {
                    emit itemsReady(batch);
                    batch.clear();
                }
            }
        }
        if (!batch.isEmpty())
            emit itemsReady(batch);

        FolderNodeList cached;
        cached.reserve(m_index.folderCount());
        for (auto it = m_index.folders().cbegin(); it != m_index.folders().cend(); ++it)
            cached.append(it.value().node);
        emit structureReady(cached);
        emit indexLoaded(videos, m_index.folderCount());
        emit progressChanged(videos, videos, m_rootAbsolute);
        qCInfo(lcScan) << "published cached index:" << videos << "videos," << cached.size() << "folders";

        // Load-only never walks the disk; it just restores the last result.
        if (m_mode == Mode::LoadOnly) {
            m_videoCount = videos;
            return;
        }
    }

    if (m_mode == Mode::LoadOnly) {
        qCInfo(lcScan) << "no cached index available, nothing to load";
        return;
    }

    scanDirectory(m_rootAbsolute, 0);
    flushItems(true);

    // Folders that vanished from the disk disappear from both the index and the
    // model, so a deleted directory never leaves orphaned cards behind.
    QStringList stale;
    if (m_haveIndex) {
        for (auto it = m_index.folders().cbegin(); it != m_index.folders().cend(); ++it) {
            if (!m_folders.contains(it.key()))
                stale.append(it.key());
        }
        for (const QString &path : stale) {
            emit folderItemsChanged(path, MediaItemList());
            m_index.remove(path);
            m_indexDirty = true;
        }
    }

    const FolderNodeList list = folderList();
    // A background check that found nothing must stay invisible: replacing the
    // folder tree would make the sidebar flicker every few minutes.
    if (m_mode != Mode::Verify || m_indexDirty || !stale.isEmpty())
        emit structureReady(list);
    emit progressChanged(m_scannedFiles, m_videoCount, m_rootAbsolute);

    const bool cancelled = m_cancelFlag.loadRelaxed() != 0;
    qCInfo(lcScan) << "scan finished (" << modeName(m_mode) << "):" << m_videoCount << "videos,"
                   << m_scannedFiles << "files," << list.size() << "folders";

    if (!cancelled && m_indexDirty && !m_indexFile.isEmpty()) {
        m_index.setRootPath(m_rootAbsolute);
        if (m_index.save(m_indexFile))
            qCInfo(lcScan) << "index saved with" << m_index.folderCount() << "folders";
    }
}

void LibraryScanner::scanDirectory(const QString &dirPath, int depth)
{
    if (m_cancelFlag.loadRelaxed() || depth > kMaxDepth)
        return;

    const QDir dir(dirPath);
    const QString absPath = dir.absolutePath();
    const QString folderName = dir.dirName();
    const qint64 modifiedMs = directoryModifiedMs(dirPath);

    // --- unchanged directory: reuse the snapshot, only touch its children ---
    if (m_deltaMode) {
        const LibraryFolderSnapshot *cached = m_index.find(absPath);
        if (cached && cached->node.depth == depth && cached->modifiedMs == modifiedMs) {
            FolderNode node = cached->node;
            // `tagCounts` in the node are the recursive totals; rollUpFolder()
            // re-merges them from the direct counts.
            node.tagCounts = cached->directTagCounts;
            m_folders.insert(absPath, node);
            m_videoCount += cached->items.size();

            const QStringList children = cached->node.childPaths;
            for (const QString &child : children) {
                if (m_cancelFlag.loadRelaxed())
                    break;
                scanDirectory(child, depth + 1);
            }

            rollUpFolder(absPath);
            return;
        }
    }

    PathUtils::DirIndex index;
    index.build(dirPath);
    if (!index.isValid())
        return;

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
    QHash<QString, int> localTagCounts;

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
            item.premiered = nfo.premiered;
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

        for (const QString &tag : item.tags)
            localTagCounts[tag] += 1;

        ownItems.append(item);
        ++m_videoCount;
    }

    // Commit the folder node (never keep a reference across hash mutations).
    {
        FolderNode node = m_folders.value(absPath);
        node.path = absPath;
        node.name = folderName.isEmpty() ? absPath : folderName;
        node.parentPath = depth > 0 ? QFileInfo(absPath).path() : QString();
        node.depth = depth;
        node.childPaths = childDirs;
        node.directVideoCount = ownItems.size();
        node.posterPath = folderPoster;
        node.tagCounts = localTagCounts;
        m_folders.insert(absPath, node);
    }

    LibraryFolderSnapshot snapshot;
    snapshot.node = m_folders.value(absPath);
    snapshot.directTagCounts = localTagCounts;
    snapshot.modifiedMs = modifiedMs;
    snapshot.items = ownItems;
    m_index.insert(snapshot);
    m_indexDirty = true;

    // During a fresh scan the folder tree is published as it grows: named
    // placeholder nodes give the sidebar something to render before the child
    // is actually read, so the user can already see and open folders instead of
    // staring at an empty page until the whole (possibly slow) walk finishes.
    if (!m_deltaMode && !isBackground()) {
        for (const QString &child : childDirs) {
            if (!m_folders.contains(child)) {
                FolderNode placeholder;
                placeholder.path = child;
                placeholder.name = QFileInfo(child).fileName();
                placeholder.parentPath = absPath;
                placeholder.depth = depth + 1;
                m_folders.insert(child, placeholder);
            }
        }
        maybeEmitStructure();
    }

    if (m_deltaMode) {
        // The model may still hold the previous version of this folder.
        emit folderItemsChanged(absPath, ownItems);
    } else {
        m_pendingItems += ownItems;
        flushItems(false);
    }
    emitProgress(false);

    for (const QString &child : childDirs) {
        if (m_cancelFlag.loadRelaxed())
            break;
        scanDirectory(child, depth + 1);
    }

    rollUpFolder(absPath);
}

void LibraryScanner::rollUpFolder(const QString &absPath)
{
    const FolderNode node = m_folders.value(absPath);

    int total = node.directVideoCount;
    QHash<QString, int> mergedTags = node.tagCounts;
    for (const QString &child : node.childPaths) {
        const FolderNode childNode = m_folders.value(child);
        total += childNode.totalVideoCount;
        for (auto it = childNode.tagCounts.cbegin(); it != childNode.tagCounts.constEnd(); ++it)
            mergedTags[it.key()] = mergedTags.value(it.key()) + it.value();
    }

    FolderNode &target = m_folders[absPath];
    target.totalVideoCount = total;
    target.tagCounts = mergedTags;
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

void LibraryScanner::maybeEmitStructure()
{
    if (isBackground() || m_deltaMode)
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastStructureMs < kStructureIntervalMs)
        return;
    m_lastStructureMs = now;
    emit structureReady(folderList());
}
