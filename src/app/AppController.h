#pragma once

#include <QHash>
#include <QObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "core/FolderNode.h"
#include "core/MediaItem.h"
// The pointer-typed Q_PROPERTIES below need complete types for the generated
// meta object code.
#include "model/FolderListModel.h"
#include "model/MediaFilterProxyModel.h"
#include "model/MediaItemModel.h"
#include "player/PlayerController.h"
#include "settings/SettingsStore.h"
#include "thumb/ThumbnailService.h"

class LibraryScanner;
class MpvEngine;
class ThumbnailCache;
class QQuickWindow;

/**
 * Facade that the QML layer talks to. It owns the library data, the scanning
 * worker, the thumbnail service and the player backend, and exposes a flat set
 * of properties/slots so the views stay declarative.
 */
class AppController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(SettingsStore *settings READ settings CONSTANT)
    Q_PROPERTY(MediaFilterProxyModel *mediaModel READ mediaModel CONSTANT)
    /** Flat list of every video in the library, unfiltered (the playlist). */
    Q_PROPERTY(MediaItemModel *allVideos READ allVideos CONSTANT)
    Q_PROPERTY(FolderListModel *childFolders READ childFolders CONSTANT)
    Q_PROPERTY(PlayerController *player READ player NOTIFY playerChanged)
    Q_PROPERTY(ThumbnailService *thumbnails READ thumbnails CONSTANT)

    Q_PROPERTY(QString libraryRoot READ libraryRoot WRITE setLibraryRoot NOTIFY libraryRootChanged)
    Q_PROPERTY(bool libraryReady READ libraryReady NOTIFY libraryRootChanged)
    /** When the media library was last scanned, or empty when never scanned. */
    Q_PROPERTY(QString lastScanTime READ lastScanTime NOTIFY lastScanTimeChanged)
    /** True when there is no scan index yet and the user should scan. */
    Q_PROPERTY(bool needsScan READ needsScan NOTIFY needsScanChanged)

    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int scannedFiles READ scannedFiles NOTIFY progressChanged)
    Q_PROPERTY(int scannedVideos READ scannedVideos NOTIFY progressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    /** Current sort key of the library grid ("scan", "title", ... "release"). */
    Q_PROPERTY(QString gridSortField READ gridSortField NOTIFY gridSortFieldChanged)
    Q_PROPERTY(QString currentFolder READ currentFolder NOTIFY currentFolderChanged)
    Q_PROPERTY(QString currentFolderName READ currentFolderName NOTIFY currentFolderChanged)
    Q_PROPERTY(QVariantList breadcrumb READ breadcrumb NOTIFY breadcrumbChanged)
    Q_PROPERTY(int folderCount READ folderCount NOTIFY folderCountChanged)
    Q_PROPERTY(bool playlistVisible READ playlistVisible WRITE setPlaylistVisible NOTIFY playlistVisibleChanged)
    Q_PROPERTY(bool detailsVisible READ detailsVisible WRITE setDetailsVisible NOTIFY detailsVisibleChanged)

    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY visibleCountChanged)
    Q_PROPERTY(QStringList selectedTags READ selectedTags WRITE setSelectedTags NOTIFY selectedTagsChanged)
    Q_PROPERTY(QStringList availableTags READ availableTags NOTIFY availableTagsChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)

    Q_PROPERTY(QVariantMap currentItem READ currentItem NOTIFY currentItemChanged)
    Q_PROPERTY(QString playbackError READ playbackError NOTIFY playerChanged)

    /**
     * Where the video picture is drawn right now, in window coordinates.
     *
     * The QML page owns a single MpvItem for the whole window instead of one
     * per page: destroying the item also destroys mpv's render context, which
     * aborts playback. The item follows this rectangle, so the player page and
     * the picture-in-picture window only publish where they want it.
     */
    Q_PROPERTY(QRectF videoRect READ videoRect NOTIFY videoRectChanged)
    Q_PROPERTY(bool videoVisible READ videoVisible NOTIFY videoVisibleChanged)
    Q_PROPERTY(bool fullscreen READ isFullscreen NOTIFY fullscreenChanged)

    // Platform capabilities / packaging mode, both fixed for a given build.
    Q_PROPERTY(bool embeddedSupported READ embeddedSupported CONSTANT)
    Q_PROPERTY(bool portable READ isPortable CONSTANT)
    Q_PROPERTY(QString settingsPath READ settingsPath CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    SettingsStore *settings() const { return m_settings; }
    MediaFilterProxyModel *mediaModel() const { return m_mediaModel; }
    MediaItemModel *allVideos() const { return m_itemModel; }
    FolderListModel *childFolders() const { return m_childFolders; }
    PlayerController *player() const { return m_player; }
    ThumbnailService *thumbnails() const { return m_thumbService; }

    QString libraryRoot() const;
    bool libraryReady() const;
    QString lastScanTime() const { return m_lastScanTime; }
    bool needsScan() const { return m_needsScan; }

    bool scanning() const { return m_scanning; }
    int scannedFiles() const { return m_scannedFiles; }
    int scannedVideos() const { return m_scannedVideos; }
    QString statusMessage() const { return m_statusMessage; }
    QString errorMessage() const { return m_errorMessage; }

    QString viewMode() const { return m_viewMode; }
    QString gridSortField() const { return m_gridSortField; }
    QString currentFolder() const { return m_currentFolder; }
    QString currentFolderName() const;
    QVariantList breadcrumb() const { return m_breadcrumb; }
    int folderCount() const { return m_folderCount; }
    bool playlistVisible() const { return m_playlistVisible; }
    bool detailsVisible() const { return m_detailsVisible; }

    int visibleCount() const;
    QStringList selectedTags() const;
    QStringList availableTags() const;
    QString searchText() const;

    QVariantMap currentItem() const { return m_currentItem; }
    QString playbackError() const;

    QRectF videoRect() const { return m_videoRectDip; }
    bool videoVisible() const { return m_videoVisible; }
    bool isFullscreen() const { return m_fullscreen; }

    /** False where ffplay cannot be hosted inside our own window (Linux). */
    bool embeddedSupported() const;
    /** True when the settings live next to the executable (portable build). */
    bool isPortable() const;
    /** Full path of the ini file, shown in the settings page. */
    QString settingsPath() const;

    // QML drives these directly (search field, view switch, folder pickers), so
    // they have to be invokable: being a Q_PROPERTY WRITE accessor is not enough.
    Q_INVOKABLE void setLibraryRoot(const QString &path);
    Q_INVOKABLE void setViewMode(const QString &mode);
    /** Sets the library-grid sort field and applies it. */
    Q_INVOKABLE void setGridSort(const QString &field);
    /** Opens the library filtered to every video the given actor appears in. */
    Q_INVOKABLE void openActor(const QString &actor);
    Q_INVOKABLE void setSelectedTags(const QStringList &tags);
    Q_INVOKABLE void setSearchText(const QString &text);
    Q_INVOKABLE void setPlaylistVisible(bool visible);
    Q_INVOKABLE void setDetailsVisible(bool visible);

public slots:
    /** Called from QML once the application window exists. */
    void attachWindow(QObject *window);

    void rescan();
    /** Ignores the scan index and reads every folder again (settings action). */
    Q_INVOKABLE void rescanFull();
    void openFolder(const QString &folderPath);
    void navigateUp();
    void navigateToBreadcrumb(int index);
    void goHome();

    void toggleTag(const QString &tag);
    void clearTags();

    void playItem(const QString &filePath);
    void playRow(int row);
    void stopPlayback();
    void togglePlayPause();
    void seekTo(double seconds);
    void seekRelative(double deltaSeconds);
    void setVolume(int volume);
    void toggleMute();

    void setVideoRect(qreal x, qreal y, qreal width, qreal height);
    void setVideoVisible(bool visible);
    void setSpeed(double speed);
    void toggleFullscreen();

    void setToolDirectory(const QString &dir);
    void setPlayMode(const QString &mode);
    void setThemeMode(const QString &mode);
    void clearThumbnailCache();
    /** Queues generation of every missing cover (settings action). */
    Q_INVOKABLE void generateAllCovers();
    void openWithSystemPlayer(const QString &filePath);
    void revealInFileManager(const QString &filePath);

    /** Describes what would be removed for the given video (confirmation UI). */
    Q_INVOKABLE QVariantMap deletePreview(const QString &filePath) const;
    /** Deletes the video (and, when the folder holds nothing else, the folder). */
    Q_INVOKABLE bool deleteItem(const QString &filePath);

    /** Returns an empty string when the directory is usable, an error otherwise. */
    Q_INVOKABLE QString checkToolDirectory(const QString &dir) const;
    /** First directory on PATH that contains ffplay/ffmpeg, or empty. */
    Q_INVOKABLE QString detectToolDir() const;
    Q_INVOKABLE QVariantMap itemAt(int row) const;
    Q_INVOKABLE QString formatDuration(double seconds) const;

    /** Cycles standalone -> embedded -> system -> standalone. */
    Q_INVOKABLE QString nextPlayMode() const;
    /** Human readable label for a play mode key. */
    Q_INVOKABLE QString playModeLabel(const QString &mode) const;

    /** Converts a `file:///...` url coming from a dialog into a local path. */
    Q_INVOKABLE QString toLocalPath(const QString &urlOrPath) const;

    /**
     * Grid scroll position persistence.
     *
     * The library grid reports its `contentY` through reportGridScroll() while
     * the user scrolls; commitGridScroll() stamps that value onto the current
     * browsing scope (whole library or one folder) right before the scope
     * changes, and gridScrollForCurrentFolder() hands it back once the grid is
     * rebuilt. This keeps the scroll position across folder navigation and page
     * switches without the QML layer having to know when a scope change starts.
     */
    Q_INVOKABLE void reportGridScroll(int contentY);
    Q_INVOKABLE void commitGridScroll();
    Q_INVOKABLE int gridScrollForCurrentFolder() const;

signals:
    void playerChanged();
    void videoRectChanged();
    void videoVisibleChanged();
    void fullscreenChanged();
    void libraryRootChanged();
    void lastScanTimeChanged();
    void needsScanChanged();
    void scanningChanged();
    void progressChanged();
    void statusMessageChanged();
    void errorMessageChanged();
    void viewModeChanged();
    void gridSortFieldChanged();
    void currentFolderChanged();
    void breadcrumbChanged();
    void folderCountChanged();
    void playlistVisibleChanged();
    void detailsVisibleChanged();
    void visibleCountChanged();
    void selectedTagsChanged();
    void availableTagsChanged();
    void searchTextChanged();
    void currentItemChanged();
    void thumbnailReady(const QString &filePath);
    void playbackRequested(const QString &filePath);
    /** The user asked to browse one actor's videos: switch to the library. */
    void actorBrowseRequested();

private:
    void wireSettings();
    void startScan();
    /** Restores the cached scan index without touching the disk. */
    void loadLibrary();
    void updateLastScanTime();
    void rebuildPlayer();
    /**
     * The embedded backend renders through one shared libmpv instance, so it
     * survives player rebuilds (switching modes) and is created on demand.
     */
    MpvEngine *ensureMpvEngine();
    /** Shows the ffmpeg/ffplay error only when the active backend needs them. */
    void refreshToolError();
    /** The active backend, or null when playback is owned by another app. */
    PlayerController *controllablePlayer() const;
    void rebuildBreadcrumb();
    void updateCurrentFolderModel();
    void setStatus(const QString &message);
    void setError(const QString &message);
    /** Stable key for the grid scroll position of the current browsing scope. */
    QString gridScrollKey() const;

    /** Drops everything the current scan produced (model, folders, counters). */
    void resetLibraryView();
    void onItemsReady(const MediaItemList &items);
    void onFolderItemsChanged(const QString &folderPath, const MediaItemList &items);
    void onStructureReady(const FolderNodeList &folders);
    void onScanFinished(int totalVideos, bool cancelled);

    SettingsStore *m_settings = nullptr;
    LibraryScanner *m_scanner = nullptr;
    MediaItemModel *m_itemModel = nullptr;
    MediaFilterProxyModel *m_mediaModel = nullptr;
    FolderListModel *m_childFolders = nullptr;
    ThumbnailCache *m_thumbCache = nullptr;
    ThumbnailService *m_thumbService = nullptr;
    PlayerController *m_player = nullptr;
    MpvEngine *m_mpvEngine = nullptr;

    QQuickWindow *m_window = nullptr;
    quintptr m_windowHandle = 0;
    QRectF m_videoRectDip;
    bool m_videoVisible = false;
    bool m_fullscreen = false;

    /** Set when a (re)scan was requested while another one was still running. */
    bool m_restartScan = false;
    bool m_restartFull = false;

    QHash<QString, FolderNode> m_folderNodes;
    QHash<QString, QString> m_folderCoverHints;

    /** Grid scroll position per browsing scope (folder path, or "flat"). */
    QHash<QString, int> m_gridScrollPositions;
    /** Most recent `contentY` reported by the grid. */
    int m_lastReportedScroll = 0;

    QString m_viewMode = QStringLiteral("flat");
    QString m_gridSortField = QStringLiteral("scan");
    QString m_currentFolder;
    QVariantList m_breadcrumb;
    QVariantMap m_currentItem;
    int m_folderCount = 0;
    bool m_playlistVisible = false;
    bool m_detailsVisible = true;
    bool m_scanning = false;
    int m_scannedFiles = 0;
    int m_scannedVideos = 0;
    QString m_statusMessage;
    QString m_errorMessage;
    QString m_lastScanTime;
    bool m_needsScan = false;
};
