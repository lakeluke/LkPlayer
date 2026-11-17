#include "app/AppController.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include "app/AppPaths.h"
#include "core/LibraryScanner.h"
#include "core/PathUtils.h"
#include "model/FolderListModel.h"
#include "model/MediaFilterProxyModel.h"
#include "model/MediaItemModel.h"
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

    m_player = PlayerFactory::create(m_settings->playMode(), m_settings->ffplayPath(), this);
    connect(m_player, &PlayerController::errorChanged, this, &AppController::playerChanged);

    m_thumbService->setFfmpegPath(m_settings->ffmpegPath());
    m_thumbCache->prune();

    // Periodic, very cheap re-check: only directory timestamps are compared, so
    // adding a file behind the app's back shows up without a full rescan.
    m_verifyTimer = new QTimer(this);
    m_verifyTimer->setInterval(3 * 60 * 1000);
    connect(m_verifyTimer, &QTimer::timeout, this, &AppController::verifyLibrary);
    m_verifyTimer->start();

    refreshToolError();

    const QString root = m_settings->libraryRoot();
    if (!root.isEmpty() && QFileInfo(root).isDir()) {
        // Defer the initial scan until the event loop is running.
        QMetaObject::invokeMethod(this, &AppController::rescan, Qt::QueuedConnection);
    }
}

AppController::~AppController() = default;

void AppController::wireSettings()
{
    connect(m_settings, &SettingsStore::libraryRootChanged, this, [this]() {
        emit libraryRootChanged();
        startScan();
    });
    connect(m_settings, &SettingsStore::ffmpegDirChanged, this, [this]() {
        m_thumbService->setFfmpegPath(m_settings->ffmpegPath());
        PlayerFactory::applyTools(m_player, m_settings->ffplayPath());
        refreshToolError();
    });
    connect(m_settings, &SettingsStore::playModeChanged, this, &AppController::rebuildPlayer);
    connect(m_settings, &SettingsStore::toolsValidChanged, this, &AppController::refreshToolError);
    connect(m_settings, &SettingsStore::viewModeChanged, this, [this]() {
        const QString mode = m_settings->viewMode();
        if (mode == m_viewMode)
            return;
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

    // Coming back to the app is a good moment to check whether the library
    // changed while it was in the background.
    connect(quickWindow, &QQuickWindow::activeChanged, this, [this]() {
        if (m_window && m_window->isActive())
            verifyLibrary();
    });
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

void AppController::setSelectedTags(const QStringList &tags)
{
    m_mediaModel->setSelectedTags(tags);
}

void AppController::setSearchText(const QString &text)
{
    m_mediaModel->setSearchText(text);
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
    }

    if (!m_folderCoverHints.isEmpty())
        m_childFolders->setCoverHints(m_folderCoverHints);
}

void AppController::onFolderItemsChanged(const QString &folderPath, const MediaItemList &items)
{
    m_itemModel->replaceFolder(folderPath, items);

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
    // Whatever just ran, the library is current: give the activation triggered
    // check a moment of rest.
    m_lastVerifyMs = QDateTime::currentMSecsSinceEpoch();

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

    if (m_scanner->isBackground()) {
        qCInfo(lcApp) << "background check finished, videos:" << totalVideos;
        return;
    }

    m_scanning = false;
    emit scanningChanged();

    if (cancelled) {
        setStatus(QStringLiteral("扫描已取消"));
    } else {
        setStatus(QStringLiteral("扫描完成：共 %1 个视频，%2 个文件夹")
                      .arg(totalVideos)
                      .arg(m_folderNodes.size()));
    }
    qCInfo(lcApp) << "scan finished, videos:" << totalVideos << "folders:" << m_folderNodes.size();
}

void AppController::verifyLibrary()
{
    if (!m_scanner || m_scanner->isRunning())
        return;
    if (m_settings->libraryRoot().isEmpty())
        return;
    // The check stats every folder, so only run it while the user is actually
    // looking at the app instead of keeping a spinning disk busy in the
    // background.
    if (m_window && !m_window->isActive())
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastVerifyMs < 30000)
        return;
    m_lastVerifyMs = now;

    m_scanner->startWithMode(LibraryScanner::Mode::Verify);
}

void AppController::openFolder(const QString &folderPath)
{
    const QString cleaned = QDir::cleanPath(folderPath);
    if (cleaned.isEmpty())
        return;

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

    if (m_player) {
        m_player->stop();
        m_player->deleteLater();
    }

    m_player = PlayerFactory::create(mode, m_settings->ffplayPath(), this);
    if (m_player) {
        m_player->setWindowHandle(m_windowHandle);
        connect(m_player, &PlayerController::errorChanged, this, &AppController::playerChanged);
    }
    refreshToolError();
    emit playerChanged();
}

void AppController::refreshToolError()
{
    // The system backend hands the file to another application, so a missing
    // ffmpeg/ffplay is not an error there.
    const bool required = !m_player || m_player->mode() != QLatin1String("system");
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

    // The system backend hands the file to another application, so ffmpeg/ffplay
    // are not required to start playback.
    const bool needsTools = !m_player || m_player->mode() != QLatin1String("system");
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
    if (m_player)
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
    if (auto *backend = controllablePlayer())
        backend->setVolume(volume);
}

void AppController::toggleMute()
{
    if (auto *backend = controllablePlayer())
        backend->toggleMute();
}

void AppController::setVideoRect(qreal x, qreal y, qreal width, qreal height)
{
    if (!m_player)
        return;

    m_videoRectDip = QRectF(x, y, width, height);
    const qreal dpr = m_window ? m_window->devicePixelRatio() : 1.0;
    m_player->setVideoRect(QRectF(x * dpr, y * dpr, width * dpr, height * dpr));
}

void AppController::setVideoVisible(bool visible)
{
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

QString AppController::checkToolDirectory(const QString &dir) const
{
    return m_settings->checkDirectory(dir);
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
