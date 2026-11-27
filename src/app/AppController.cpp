#include "app/AppController.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

#include "app/AppPaths.h"
#include "core/LibraryScanner.h"
#include "core/PathUtils.h"
#include "model/FolderListModel.h"
#include "model/MediaFilterProxyModel.h"
#include "model/MediaItemModel.h"
#include "player/MpvEngine.h"
#include "player/PlayerController.h"
#include "player/PlayerFactory.h"
#include "settings/SettingsStore.h"
#include "thumb/ThumbnailCache.h"
#include "thumb/ThumbnailService.h"

Q_LOGGING_CATEGORY(lcApp, "lk.app")

namespace {

QString coverUrlFor(const MediaItem &item)
{
    if (!item.posterPath.isEmpty())
        return QUrl::fromLocalFile(item.posterPath).toString();
    return QStringLiteral("image://thumb/%1").arg(PathUtils::encodeId(item.filePath));
}

} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_settings = new SettingsStore(this);
    m_itemModel = new MediaItemModel(this);

    m_mediaModel = new MediaFilterProxyModel(this);
    m_mediaModel->setSourceModel(m_itemModel);

    m_childFolders = new FolderListModel(this);

    m_thumbCache = new ThumbnailCache(this);
    m_thumbService = new ThumbnailService(m_thumbCache, this);
    connect(m_thumbService, &ThumbnailService::thumbnailReady, this, &AppController::thumbnailReady);

    m_scanner = new LibraryScanner(this);
    connect(m_scanner, &LibraryScanner::scanStarted, this, [this]() {
        // Drop stale metadata; onItemsReady() re-registers it as results arrive.
        m_thumbService->clearItemMetadata();

        // Background checks are invisible: they must not flip the scanning
        // indicator or overwrite the status line.
        if (m_scanner->isBackground())
            return;
        m_scanning = true;
        emit scanningChanged();
        setError(QString());
        setStatus(QStringLiteral("正在扫描媒体库…"));
    });
    connect(m_scanner, &LibraryScanner::itemsReady, this, &AppController::onItemsReady);
    connect(m_scanner, &LibraryScanner::folderItemsChanged, this, &AppController::onFolderItemsChanged);
    connect(m_scanner, &LibraryScanner::indexLoaded, this, [this](int videos, int folders) {
        if (videos <= 0)
            return;
        setStatus(QStringLiteral("已载入上次扫描结果：%1 个视频 / %2 个文件夹，正在校验目录…")
                      .arg(videos)
                      .arg(folders));
    });
    connect(m_scanner, &LibraryScanner::structureReady, this, &AppController::onStructureReady);
    connect(m_scanner, &LibraryScanner::progressChanged, this, [this](int files, int videos, const QString &) {
        m_scannedFiles = files;
        m_scannedVideos = videos;
        emit progressChanged();
    });
    connect(m_scanner, &LibraryScanner::scanFinished, this, &AppController::onScanFinished);
    connect(m_scanner, &LibraryScanner::errorOccurred, this, [this](const QString &message) {
        m_scanning = false;
        emit scanningChanged();
        setError(message);
    });

    connect(m_mediaModel, &MediaFilterProxyModel::countChanged, this, &AppController::visibleCountChanged);
    connect(m_mediaModel, &MediaFilterProxyModel::availableTagsChanged, this, &AppController::availableTagsChanged);
    connect(m_mediaModel, &MediaFilterProxyModel::selectedTagsChanged, this, &AppController::selectedTagsChanged);
    connect(m_mediaModel, &MediaFilterProxyModel::searchTextChanged, this, &AppController::searchTextChanged);
    connect(m_mediaModel, &MediaFilterProxyModel::viewModeChanged, this, &AppController::viewModeChanged);
    connect(m_mediaModel, &MediaFilterProxyModel::currentFolderChanged, this, &AppController::currentFolderChanged);

    connect(m_childFolders, &FolderListModel::countChanged, this, &AppController::folderCountChanged);

    // Restore persisted state.
    m_settings->load();
    m_viewMode = m_settings->viewMode();
    m_mediaModel->setViewMode(m_viewMode);

    wireSettings();

    m_player = PlayerFactory::create(m_settings->playMode(), m_settings->ffplayPath(), ensureMpvEngine(), this);
    connect(m_player, &PlayerController::errorChanged, this, &AppController::playerChanged);
    if (m_player)
        m_player->setVolume(m_settings->volume());

    m_thumbService->setFfmpegPath(m_settings->ffmpegPath());
    m_thumbService->setGenerationEnabled(m_settings->generateCovers());
    m_thumbCache->prune();

    refreshToolError();

    // Never scan automatically: restore the cached index (LoadOnly) and only
    // prompt the user when there is nothing cached yet.
    QMetaObject::invokeMethod(this, &AppController::loadLibrary, Qt::QueuedConnection);
}

AppController::~AppController()
{
    // Flush any debounced writes (e.g. volume) that are still pending on exit.
    if (m_settings)
        m_settings->sync();
}

void AppController::wireSettings()
{
    connect(m_settings, &SettingsStore::libraryRootChanged, this, [this]() {
        emit libraryRootChanged();
        loadLibrary();
    });
    connect(m_settings, &SettingsStore::ffmpegDirChanged, this, [this]() {
        m_thumbService->setFfmpegPath(m_settings->ffmpegPath());
        PlayerFactory::applyTools(m_player, m_settings->ffplayPath());
        refreshToolError();
    });
    connect(m_settings, &SettingsStore::playModeChanged, this, &AppController::rebuildPlayer);
    connect(m_settings, &SettingsStore::toolsValidChanged, this, &AppController::refreshToolError);
    connect(m_settings, &SettingsStore::generateCoversChanged, this, [this]() {
        m_thumbService->setGenerationEnabled(m_settings->generateCovers());
    });
    connect(m_settings, &SettingsStore::viewModeChanged, this, [this]() {
        const QString mode = m_settings->viewMode();
        if (mode == m_viewMode)
            return;
        commitGridScroll();
        m_viewMode = mode;
        m_mediaModel->setViewMode(mode);
        rebuildBreadcrumb();
        emit viewModeChanged();
    });
}

// ---------------------------------------------------------------------------
// getters
// ---------------------------------------------------------------------------

QString AppController::libraryRoot() const
{
    return m_settings->libraryRoot();
}

bool AppController::libraryReady() const
{
    const QString root = m_settings->libraryRoot();
    return !root.isEmpty() && QFileInfo(root).isDir();
}

QString AppController::currentFolderName() const
{
    if (m_viewMode != QLatin1String("drill"))
        return QStringLiteral("全部视频");
    if (m_currentFolder.isEmpty())
        return QStringLiteral("媒体库");
    const auto it = m_folderNodes.constFind(m_currentFolder);
    if (it != m_folderNodes.constEnd())
        return it.value().name;
    return QDir(m_currentFolder).dirName();
}

int AppController::visibleCount() const
{
    return m_mediaModel->count();
}

QStringList AppController::selectedTags() const
{
    return m_mediaModel->selectedTags();
}

QStringList AppController::availableTags() const
{
    return m_mediaModel->availableTags();
}

QString AppController::searchText() const
{
    return m_mediaModel->searchText();
}

QString AppController::playbackError() const
{
    return m_player ? m_player->errorText() : QString();
}

// ---------------------------------------------------------------------------
// setters / slots
// ---------------------------------------------------------------------------

void AppController::attachWindow(QObject *window)
{
    auto *quickWindow = qobject_cast<QQuickWindow *>(window);
    if (!quickWindow) {
        qCWarning(lcApp) << "attachWindow called with a non QQuickWindow object";
        return;
    }
    m_window = quickWindow;
    m_windowHandle = static_cast<quintptr>(quickWindow->winId());
    qCInfo(lcApp) << "attached to window, native handle" << m_windowHandle;
    if (m_player)
        m_player->setWindowHandle(m_windowHandle);
}

void AppController::setLibraryRoot(const QString &path)
{
    const QString cleaned = path.isEmpty() ? QString() : QDir::cleanPath(path);
    if (cleaned == m_settings->libraryRoot()) {
        rescan();
        return;
    }
    m_settings->setLibraryRoot(cleaned);
}

void AppController::setViewMode(const QString &mode)
{
    m_settings->setViewMode(mode);
}

void AppController::setGridSort(const QString &field)
{
    if (field == m_gridSortField)
        return;

    m_gridSortField = field;

    // Time-like fields read better newest / longest first.
    const bool ascending = field != QLatin1String("duration")
                           && field != QLatin1String("modified")
                           && field != QLatin1String("release");
    m_mediaModel->sortBy(field, ascending);
    emit gridSortFieldChanged();
}

void AppController::openActor(const QString &actor)
{
    QString name = actor.trimmed();
    if (name.isEmpty())
        return;

    // Actors may carry a role ("Name / Role"): search by the bare name so all
    // of the actor's appearances match regardless of the credited role.
    const int sep = name.indexOf(QStringLiteral(" / "));
    if (sep > 0)
        name = name.left(sep).trimmed();
    if (name.isEmpty())
        return;

    // Browse the whole library for this actor: clear any tag narrowing, flatten
    // the view so folders don't hide matches, then filter by the actor's name.
    m_mediaModel->setSelectedTags(QStringList());
    setViewMode(QStringLiteral("flat"));
    m_mediaModel->setSearchText(name);
    emit actorBrowseRequested();
}

void AppController::reportGridScroll(int contentY)
{
    m_lastReportedScroll = contentY;
}

void AppController::commitGridScroll()
{
    const QString key = gridScrollKey();
    if (m_lastReportedScroll > 0)
        m_gridScrollPositions.insert(key, m_lastReportedScroll);
    else
        m_gridScrollPositions.remove(key);
}

int AppController::gridScrollForCurrentFolder() const
{
    return m_gridScrollPositions.value(gridScrollKey(), 0);
}

QString AppController::gridScrollKey() const
{
    return (m_viewMode == QLatin1String("drill") && !m_currentFolder.isEmpty())
               ? QDir::cleanPath(m_currentFolder)
               : QStringLiteral("flat");
}

void AppController::setSelectedTags(const QStringList &tags)
{
    m_mediaModel->setSelectedTags(tags);
}

void AppController::setSearchText(const QString &text)
{
    m_mediaModel->setSearchText(text);
}

void AppController::setPlaylistVisible(bool visible)
{
    if (m_playlistVisible == visible)
        return;
    m_playlistVisible = visible;
    emit playlistVisibleChanged();
}

void AppController::setDetailsVisible(bool visible)
{
    if (m_detailsVisible == visible)
        return;
    m_detailsVisible = visible;
    emit detailsVisibleChanged();
}

void AppController::rescan()
{
    startScan();
}

void AppController::rescanFull()
{
    const QString root = m_settings->libraryRoot();
    if (root.isEmpty() || !QFileInfo(root).isDir()) {
        rescan();
        return;
    }

    // A background check may be running right now: stop it and come back once
    // the scanner is free again, otherwise the request would be dropped.
    if (m_scanner->isRunning()) {
        m_restartScan = true;
        m_restartFull = true;
        m_scanner->cancel();
        return;
    }

    // Re-read everything: the only way to notice a file whose content (and
    // therefore sidecar metadata) changed without touching the folder mtime.
    resetLibraryView();

    m_childFolders->setRootPath(root);
    m_scanner->setRootPath(root);
    m_scanner->setIndexFile(AppPaths::scanIndexFile(root));
    m_scanner->startWithMode(LibraryScanner::Mode::Full);
}

void AppController::startScan()
{
    const QString root = m_settings->libraryRoot();

    // Ask a running scan (usually the background check) to stop and queue this
    // request; dropping it would leave the view empty until the next attempt.
    if (m_scanner->isRunning()) {
        m_restartScan = true;
        m_restartFull = false;
        m_scanner->cancel();
        return;
    }

    m_currentFolder.clear();
    m_mediaModel->setSelectedTags(QStringList());
    m_mediaModel->setSearchText(QString());
    resetLibraryView();

    if (root.isEmpty() || !QFileInfo(root).isDir()) {
        if (!root.isEmpty())
            setError(QStringLiteral("媒体库目录不可用：%1").arg(root));
        setStatus(QStringLiteral("尚未设置媒体库目录"));
        return;
    }

    m_childFolders->setRootPath(root);
    m_scanner->setRootPath(root);
    m_scanner->setIndexFile(AppPaths::scanIndexFile(root));
    m_scanner->start();
}

void AppController::loadLibrary()
{
    const QString root = m_settings->libraryRoot();

    m_currentFolder.clear();
    m_mediaModel->setSelectedTags(QStringList());
    m_mediaModel->setSearchText(QString());
    resetLibraryView();

    if (root.isEmpty() || !QFileInfo(root).isDir()) {
        if (!root.isEmpty())
            setError(QStringLiteral("媒体库目录不可用：%1").arg(root));
        else
            setStatus(QStringLiteral("尚未设置媒体库目录"));
        updateLastScanTime();
        return;
    }

    m_childFolders->setRootPath(root);
    m_scanner->setRootPath(root);
    m_scanner->setIndexFile(AppPaths::scanIndexFile(root));
    m_scanner->startWithMode(LibraryScanner::Mode::LoadOnly);
    updateLastScanTime();
}

void AppController::updateLastScanTime()
{
    QString text;
    const QString root = m_settings->libraryRoot();
    if (!root.isEmpty()) {
        const QFileInfo info(AppPaths::scanIndexFile(root));
        if (info.exists())
            text = info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    }

    if (text == m_lastScanTime)
        return;
    m_lastScanTime = text;
    emit lastScanTimeChanged();
}

void AppController::resetLibraryView()
{
    m_itemModel->clear();
    m_folderNodes.clear();
    m_folderCoverHints.clear();
    m_childFolders->setFolders(FolderNodeList());
    m_childFolders->setCoverHints(QHash<QString, QString>());
    m_scannedFiles = 0;
    m_scannedVideos = 0;
    emit progressChanged();
    emit folderCountChanged();
    emit currentFolderChanged();
    rebuildBreadcrumb();
}

void AppController::onItemsReady(const MediaItemList &items)
{
    m_itemModel->addItems(items);

    for (const MediaItem &item : items) {
        if (!m_folderCoverHints.contains(item.folderPath))
            m_folderCoverHints.insert(item.folderPath, coverUrlFor(item));

        ThumbnailMeta meta;
        meta.posterPath = item.posterPath;
        meta.fileSize = item.fileSize;
        meta.lastModified = item.lastModified;
        m_thumbService->setItemMetadata(item.filePath, meta);
    }

    if (!m_folderCoverHints.isEmpty())
        m_childFolders->setCoverHints(m_folderCoverHints);
}

void AppController::onFolderItemsChanged(const QString &folderPath, const MediaItemList &items)
{
    m_itemModel->replaceFolder(folderPath, items);

    for (const MediaItem &item : items) {
        ThumbnailMeta meta;
        meta.posterPath = item.posterPath;
        meta.fileSize = item.fileSize;
        meta.lastModified = item.lastModified;
        m_thumbService->setItemMetadata(item.filePath, meta);
    }

    if (items.isEmpty()) {
        m_folderCoverHints.remove(folderPath);
    } else if (!m_folderCoverHints.contains(folderPath)) {
        m_folderCoverHints.insert(folderPath, coverUrlFor(items.first()));
    }
    m_childFolders->setCoverHints(m_folderCoverHints);
}

void AppController::onStructureReady(const FolderNodeList &folders)
{
    m_folderNodes.clear();
    m_folderNodes.reserve(folders.size());
    for (const FolderNode &node : folders)
        m_folderNodes.insert(node.path, node);

    m_childFolders->setFolders(folders);
    m_childFolders->setRootPath(m_settings->libraryRoot());

    // The structure is published twice (cached snapshot, then the verified
    // result), so the folder the user is browsing has to survive both.
    QString wanted = m_currentFolder;
    if (wanted.isEmpty())
        wanted = QDir::cleanPath(m_settings->lastFolder());
    if (m_viewMode == QLatin1String("drill") && !wanted.isEmpty() && m_folderNodes.contains(wanted))
        m_currentFolder = wanted;
    else
        m_currentFolder = QDir(QDir::cleanPath(m_settings->libraryRoot())).absolutePath();

    updateCurrentFolderModel();
    emit folderCountChanged();
}

void AppController::onScanFinished(int totalVideos, bool cancelled)
{
    if (m_restartScan) {
        m_restartScan = false;
        const bool full = m_restartFull;
        m_restartFull = false;
        if (full)
            rescanFull();
        else
            startScan();
        return;
    }

    // Load-only never touched the disk; if nothing was cached, ask the user
    // whether to run a real scan.
    if (m_scanner->mode() == LibraryScanner::Mode::LoadOnly) {
        const QString indexFile = AppPaths::scanIndexFile(m_settings->libraryRoot());
        const bool needsScan = !QFileInfo(indexFile).exists();
        if (needsScan != m_needsScan) {
            m_needsScan = needsScan;
            emit needsScanChanged();
        }
        updateLastScanTime();
        return;
    }

    if (m_scanner->isBackground()) {
        qCInfo(lcApp) << "background check finished, videos:" << totalVideos;
        return;
    }

    m_scanning = false;
    emit scanningChanged();

    updateLastScanTime();

    if (cancelled) {
        setStatus(QStringLiteral("扫描已取消"));
    } else {
        setStatus(QStringLiteral("扫描完成：共 %1 个视频，%2 个文件夹")
                      .arg(totalVideos)
                      .arg(m_folderNodes.size()));
    }
    qCInfo(lcApp) << "scan finished, videos:" << totalVideos << "folders:" << m_folderNodes.size();
}

void AppController::openFolder(const QString &folderPath)
{
    const QString cleaned = QDir::cleanPath(folderPath);
    if (cleaned.isEmpty())
        return;

    commitGridScroll();

    if (m_viewMode != QLatin1String("drill")) {
        m_viewMode = QStringLiteral("drill");
        m_settings->setViewMode(m_viewMode);
        m_mediaModel->setViewMode(m_viewMode);
        rebuildBreadcrumb();
        emit viewModeChanged();
    }

    m_currentFolder = cleaned;
    m_settings->setLastFolder(cleaned);
    updateCurrentFolderModel();
}

void AppController::navigateUp()
{
    if (m_viewMode != QLatin1String("drill"))
        return;

    const auto it = m_folderNodes.constFind(m_currentFolder);
    if (it == m_folderNodes.constEnd())
        return;

    const QString parent = it.value().parentPath;
    if (parent.isEmpty())
        return;

    commitGridScroll();
    m_currentFolder = parent;
    m_settings->setLastFolder(parent);
    updateCurrentFolderModel();
}

void AppController::navigateToBreadcrumb(int index)
{
    if (index < 0 || index >= m_breadcrumb.size())
        return;
    const QVariantMap entry = m_breadcrumb.at(index).toMap();
    const QString path = entry.value(QStringLiteral("path")).toString();
    if (path.isEmpty())
        return;

    commitGridScroll();

    if (m_viewMode != QLatin1String("drill")) {
        m_viewMode = QStringLiteral("drill");
        m_settings->setViewMode(m_viewMode);
        m_mediaModel->setViewMode(m_viewMode);
        rebuildBreadcrumb();
        emit viewModeChanged();
    }

    m_currentFolder = path;
    m_settings->setLastFolder(path);
    updateCurrentFolderModel();
}

void AppController::goHome()
{
    const QString root = QDir(QDir::cleanPath(m_settings->libraryRoot())).absolutePath();
    if (m_viewMode == QLatin1String("drill")) {
        commitGridScroll();
        m_currentFolder = root;
        m_settings->setLastFolder(root);
        updateCurrentFolderModel();
    } else {
        navigateToBreadcrumb(0);
    }
}

void AppController::updateCurrentFolderModel()
{
    m_childFolders->setCurrentFolder(m_currentFolder);
    m_mediaModel->setCurrentFolder(m_currentFolder);
    rebuildBreadcrumb();
    emit currentFolderChanged();
}

void AppController::rebuildBreadcrumb()
{
    QVariantList crumb;

    QVariantMap rootEntry;
    const QString rootPath = QDir(QDir::cleanPath(m_settings->libraryRoot())).absolutePath();
    const auto rootIt = m_folderNodes.constFind(rootPath);
    rootEntry.insert(QStringLiteral("name"),
                     rootIt != m_folderNodes.constEnd() ? rootIt.value().name : QStringLiteral("媒体库"));
    rootEntry.insert(QStringLiteral("path"), rootPath);
    crumb.append(rootEntry);

    if (m_viewMode == QLatin1String("drill") && !m_currentFolder.isEmpty()) {
        QStringList chain;
        QString cursor = QDir::cleanPath(m_currentFolder);
        int guard = 0;
        while (!cursor.isEmpty() && cursor.compare(rootPath, Qt::CaseInsensitive) != 0 && guard++ < 64) {
            const auto it = m_folderNodes.constFind(cursor);
            if (it == m_folderNodes.constEnd())
                break;
            chain.prepend(cursor);
            cursor = it.value().parentPath;
        }
        for (const QString &path : chain) {
            QVariantMap entry;
            const auto it = m_folderNodes.constFind(path);
            entry.insert(QStringLiteral("name"),
                         it != m_folderNodes.constEnd() ? it.value().name : QDir(path).dirName());
            entry.insert(QStringLiteral("path"), path);
            crumb.append(entry);
        }
    }

    m_breadcrumb = crumb;
    emit breadcrumbChanged();
}

void AppController::toggleTag(const QString &tag)
{
    m_mediaModel->toggleTag(tag);
}

void AppController::clearTags()
{
    m_mediaModel->clearTags();
}

// ---------------------------------------------------------------------------
// playback
// ---------------------------------------------------------------------------

void AppController::rebuildPlayer()
{
    const QString mode = m_settings->playMode();
    if (m_player && m_player->mode() == mode)
        return;

    // A backend switch tears the old player down. Remember what it was playing
    // so the new backend can resume the same file: otherwise the fresh player
    // starts idle with no file loaded and the play button becomes a no-op.
    const bool wasActive = m_player && m_player->isActive();
    const QString resumePath = m_player ? m_player->filePath() : QString();
    const QString resumeTitle = m_player ? m_player->title() : QString();
    const double resumePosition = m_player ? m_player->position() : 0.0;

    if (m_player) {
        m_player->stop();
        m_player->deleteLater();
    }

    // Leaving the embedded backend detaches the render context that MpvItem
    // owns. Tell the shared engine it is gone so the next embedded playback
    // waits for a fresh context instead of loading into a dead one. (MpvItem
    // also reports the teardown, but only when it actually renders a frame; on
    // a hidden page that never happens.)
    if (mode != QLatin1String("embedded") && m_mpvEngine)
        m_mpvEngine->notifyRenderContextLost();

    m_player = PlayerFactory::create(mode, m_settings->ffplayPath(), ensureMpvEngine(), this);
    if (m_player) {
        m_player->setWindowHandle(m_windowHandle);
        connect(m_player, &PlayerController::errorChanged, this, &AppController::playerChanged);
        m_player->setVolume(m_settings->volume());
    }
    refreshToolError();
    emit playerChanged();

    // Resume after playerChanged so MpvItem has already adopted the new player
    // and can hand its render context over before the load is attempted.
    if (m_player && wasActive && !resumePath.isEmpty()) {
        m_player->setVideoVisible(true);
        m_player->openAt(resumePath, resumeTitle, resumePosition);
    }
}

MpvEngine *AppController::ensureMpvEngine()
{
#ifdef LKPLAYER_HAVE_MPV
    if (m_mpvEngine)
        return m_mpvEngine;

    // Only the embedded mode renders through libmpv; creating the instance for
    // the other modes would start audio/video threads nobody ever uses.
    if (m_settings->playMode() != QLatin1String("embedded"))
        return nullptr;

    m_mpvEngine = new MpvEngine(this);
    if (m_mpvEngine->isValid())
        qCInfo(lcApp) << "created the shared libmpv instance";
    else
        qCWarning(lcApp) << "libmpv is unavailable:" << m_mpvEngine->lastError();
    return m_mpvEngine;
#else
    return nullptr;
#endif
}

void AppController::refreshToolError()
{
    // The system backend hands the file to another application and the embedded
    // one decodes in-process, so a missing ffmpeg/ffplay is not an error there.
    const bool required = !m_player || m_player->requiresExternalTools();
    setError(required && !m_settings->toolsValid() ? m_settings->toolsStatus() : QString());
}

PlayerController *AppController::controllablePlayer() const
{
    // The system backend cannot be driven from here, so transport commands are
    // dropped instead of moving UI state that nothing will honour.
    return (m_player && m_player->isControllable()) ? m_player : nullptr;
}

void AppController::playItem(const QString &filePath)
{
    const int row = m_itemModel->indexOfPath(filePath);
    if (row < 0) {
        qCWarning(lcApp) << "playItem: unknown file" << filePath;
        return;
    }
    playRow(row);
}

void AppController::playRow(int row)
{
    const MediaItem *entry = m_itemModel->itemAt(row);
    if (!entry)
        return;

    m_currentItem = m_itemModel->item(row);
    emit currentItemChanged();

    if (!m_player)
        rebuildPlayer();

    // Only the ffplay backends need the external tools; libmpv decodes
    // in-process and the system player lives in another application.
    const bool needsTools = !m_player || m_player->requiresExternalTools();
    if (needsTools && !m_settings->toolsValid()) {
        setError(m_settings->toolsStatus());
        return;
    }

    if (m_player) {
        m_player->setVideoVisible(true);
        m_player->open(entry->filePath, entry->title);
    }

    emit playbackRequested(entry->filePath);
}

void AppController::stopPlayback()
{
    if (m_player)
        m_player->stop();
}

void AppController::togglePlayPause()
{
    if (!m_player)
        return;

    // A backend switch leaves the player idle with no file loaded; fall back to
    // the current selection so the play button still does something useful.
    if (!m_player->isActive() && m_player->filePath().isEmpty()) {
        const QString path = m_currentItem.value(QStringLiteral("filePath")).toString();
        if (!path.isEmpty())
            playItem(path);
        return;
    }

    m_player->togglePlayPause();
}

void AppController::seekTo(double seconds)
{
    if (auto *backend = controllablePlayer())
        backend->seekTo(seconds);
}

void AppController::seekRelative(double deltaSeconds)
{
    if (auto *backend = controllablePlayer())
        backend->seekRelative(deltaSeconds);
}

void AppController::setVolume(int volume)
{
    m_settings->setVolume(volume);
    if (auto *backend = controllablePlayer())
        backend->setVolume(volume);
}

void AppController::toggleMute()
{
    if (auto *backend = controllablePlayer())
        backend->toggleMute();
}

void AppController::setSpeed(double speed)
{
    if (auto *backend = controllablePlayer())
        backend->setSpeed(speed);
}

void AppController::toggleFullscreen()
{
    if (!m_window)
        return;

    const bool enterFullscreen = m_window->visibility() != QWindow::FullScreen;
    if (enterFullscreen)
        m_window->showFullScreen();
    else
        m_window->showNormal();

    if (enterFullscreen != m_fullscreen) {
        m_fullscreen = enterFullscreen;
        emit fullscreenChanged();
    }
}

void AppController::setVideoRect(qreal x, qreal y, qreal width, qreal height)
{
    // The pages report on every layout pass; publishing an unchanged rectangle
    // would restart the transition on the video item each time.
    const QRectF rect(x, y, width, height);
    if (rect == m_videoRectDip)
        return;

    m_videoRectDip = rect;
    emit videoRectChanged();

    if (!m_player)
        return;

    const qreal dpr = m_window ? m_window->devicePixelRatio() : 1.0;
    m_player->setVideoRect(QRectF(x * dpr, y * dpr, width * dpr, height * dpr));
}

void AppController::setVideoVisible(bool visible)
{
    if (visible != m_videoVisible) {
        m_videoVisible = visible;
        emit videoVisibleChanged();
    }

    if (m_player)
        m_player->setVideoVisible(visible);
}

// ---------------------------------------------------------------------------
// settings helpers
// ---------------------------------------------------------------------------

void AppController::setToolDirectory(const QString &dir)
{
    m_settings->setFfmpegDir(dir);
}

void AppController::setPlayMode(const QString &mode)
{
    m_settings->setPlayMode(mode);
}

void AppController::setThemeMode(const QString &mode)
{
    m_settings->setThemeMode(mode);
}

void AppController::clearThumbnailCache()
{
    const int removed = m_thumbService->clearCache();
    setStatus(QStringLiteral("已清理 %1 个封面缓存").arg(removed));
}

void AppController::generateAllCovers()
{
    const int rows = m_itemModel->rowCount();
    QStringList paths;
    paths.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        const MediaItem *item = m_itemModel->itemAt(i);
        if (item && !item->filePath.isEmpty())
            paths << item->filePath;
    }

    setStatus(QStringLiteral("正在后台生成封面…"));

    // The missing-cover scan reads the SQLite cache (and poster files) and can
    // take a while for a large library, so run it off the GUI thread. The task
    // is fire-and-forget (its result is reported back through the queued
    // invokeMethod below), so the QFuture is intentionally not tracked.
    [[maybe_unused]] const auto coversFuture = QtConcurrent::run([this, paths = std::move(paths)]() {
        const int queued = m_thumbService->requestMissingGeneration(paths);
        QMetaObject::invokeMethod(this, [this, queued]() {
            if (queued > 0)
                setStatus(QStringLiteral("已加入 %1 个封面生成任务").arg(queued));
            else
                setStatus(QStringLiteral("没有缺失的封面需要生成"));
        }, Qt::QueuedConnection);
    });
}

QString AppController::checkToolDirectory(const QString &dir) const
{
    return m_settings->checkDirectory(dir);
}

QString AppController::detectToolDir() const
{
    return SettingsStore::detectToolDir();
}

void AppController::openWithSystemPlayer(const QString &filePath)
{
    if (filePath.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void AppController::revealInFileManager(const QString &filePath)
{
    if (filePath.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filePath).absolutePath()));
}

QVariantMap AppController::deletePreview(const QString &filePath) const
{
    QVariantMap result;
    result.insert(QStringLiteral("deleteFolder"), false);
    result.insert(QStringLiteral("targets"), QStringList());

    const int row = m_itemModel->indexOfPath(filePath);
    if (row < 0)
        return result;
    const MediaItem *item = m_itemModel->itemAt(row);
    if (!item || item->folderPath.isEmpty())
        return result;

    // Every sidecar asset that travels with the video.
    QStringList assets;
    if (!item->posterPath.isEmpty())
        assets << item->posterPath;
    if (!item->nfoPath.isEmpty())
        assets << item->nfoPath;
    if (!item->fanartPath.isEmpty())
        assets << item->fanartPath;
    assets << item->subtitles;
    assets << item->stillImages;
    assets.removeAll(QString());
    assets.removeDuplicates();

    // When the folder holds nothing but the video and its own assets, remove the
    // whole folder. Otherwise only remove the video file and leave shared
    // folder-level assets (poster, movie.nfo) and unrelated files untouched.
    const QDir dir(item->folderPath);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    QSet<QString> relatedNames;
    relatedNames.insert(QFileInfo(item->filePath).fileName());
    for (const QString &asset : assets)
        relatedNames.insert(QFileInfo(asset).fileName());

    bool hasSubdirs = false;
    bool hasOtherFiles = false;
    for (const QFileInfo &entry : entries) {
        if (entry.isDir())
            hasSubdirs = true;
        else if (!relatedNames.contains(entry.fileName()))
            hasOtherFiles = true;
    }

    const bool deleteFolder = !hasSubdirs && !hasOtherFiles;
    QStringList targets;
    targets.append(deleteFolder ? dir.absolutePath() : item->filePath);

    result.insert(QStringLiteral("deleteFolder"), deleteFolder);
    result.insert(QStringLiteral("targets"), targets);
    return result;
}

bool AppController::deleteItem(const QString &filePath)
{
    const QVariantMap preview = deletePreview(filePath);
    const QStringList targets = preview.value(QStringLiteral("targets")).toStringList();
    if (targets.isEmpty())
        return false;

    // Stop before removing a file we might currently be playing.
    if (m_player && m_player->filePath() == filePath)
        stopPlayback();

    bool ok = true;
    for (const QString &target : targets) {
        const QFileInfo info(target);
        if (info.isDir()) {
            if (!QDir(target).removeRecursively())
                ok = false;
        } else if (QFile::exists(target) && !QFile::remove(target)) {
            ok = false;
        }
    }

    if (ok) {
        // Refresh the library in place so the removed card disappears at once.
        if (m_scanner && !m_scanner->isRunning())
            m_scanner->startWithMode(LibraryScanner::Mode::Verify);
    }

    return ok;
}

QVariantMap AppController::itemAt(int row) const
{
    return m_itemModel->item(row);
}

QString AppController::formatDuration(double seconds) const
{
    return PathUtils::formatDuration(seconds);
}

bool AppController::embeddedSupported() const
{
    return PlayerFactory::supportsEmbedded();
}

bool AppController::isPortable() const
{
    return AppPaths::isPortable();
}

QString AppController::settingsPath() const
{
    return m_settings ? m_settings->configFilePath() : QString();
}

QString AppController::nextPlayMode() const
{
    const QString mode = m_settings->playMode();
    // Skip the embedded backend where the platform cannot host a foreign window.
    if (!embeddedSupported() && mode == QLatin1String("embedded"))
        return QStringLiteral("standalone");
    if (mode == QLatin1String("standalone"))
        return embeddedSupported() ? QStringLiteral("embedded") : QStringLiteral("system");
    if (mode == QLatin1String("embedded"))
        return QStringLiteral("system");
    return QStringLiteral("standalone");
}

QString AppController::playModeLabel(const QString &mode) const
{
    if (mode == QLatin1String("embedded"))
        return QStringLiteral("内嵌模式");
    if (mode == QLatin1String("system"))
        return QStringLiteral("系统播放器");
    return QStringLiteral("独立窗口");
}

QString AppController::toLocalPath(const QString &urlOrPath) const
{
    if (urlOrPath.isEmpty())
        return QString();
    const QUrl url(urlOrPath);
    if (url.isLocalFile())
        return url.toLocalFile();
    return urlOrPath;
}

void AppController::setStatus(const QString &message)
{
    if (message == m_statusMessage)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void AppController::setError(const QString &message)
{
    if (message == m_errorMessage)
        return;
    m_errorMessage = message;
    emit errorMessageChanged();
}
