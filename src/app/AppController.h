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
#include "player/PlayerController.h"
#include "settings/SettingsStore.h"
#include "thumb/ThumbnailService.h"

class LibraryScanner;
class MediaItemModel;
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
    Q_PROPERTY(FolderListModel *childFolders READ childFolders CONSTANT)
    Q_PROPERTY(PlayerController *player READ player NOTIFY playerChanged)
    Q_PROPERTY(ThumbnailService *thumbnails READ thumbnails CONSTANT)

    Q_PROPERTY(QString libraryRoot READ libraryRoot WRITE setLibraryRoot NOTIFY libraryRootChanged)
    Q_PROPERTY(bool libraryReady READ libraryReady NOTIFY libraryRootChanged)

    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int scannedFiles READ scannedFiles NOTIFY progressChanged)
    Q_PROPERTY(int scannedVideos READ scannedVideos NOTIFY progressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(QString currentFolder READ currentFolder NOTIFY currentFolderChanged)
    Q_PROPERTY(QString currentFolderName READ currentFolderName NOTIFY currentFolderChanged)
    Q_PROPERTY(QVariantList breadcrumb READ breadcrumb NOTIFY breadcrumbChanged)
    Q_PROPERTY(int folderCount READ folderCount NOTIFY folderCountChanged)

    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY visibleCountChanged)
    Q_PROPERTY(QStringList selectedTags READ selectedTags WRITE setSelectedTags NOTIFY selectedTagsChanged)
    Q_PROPERTY(QStringList availableTags READ availableTags NOTIFY availableTagsChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)

    Q_PROPERTY(QVariantMap currentItem READ currentItem NOTIFY currentItemChanged)
    Q_PROPERTY(QString playbackError READ playbackError NOTIFY playerChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    SettingsStore *settings() const { return m_settings; }
    MediaFilterProxyModel *mediaModel() const { return m_mediaModel; }
    FolderListModel *childFolders() const { return m_childFolders; }
    PlayerController *player() const { return m_player; }
    ThumbnailService *thumbnails() const { return m_thumbService; }

    QString libraryRoot() const;
    bool libraryReady() const;

    bool scanning() const { return m_scanning; }
    int scannedFiles() const { return m_scannedFiles; }
    int scannedVideos() const { return m_scannedVideos; }
    QString statusMessage() const { return m_statusMessage; }
    QString errorMessage() const { return m_errorMessage; }

    QString viewMode() const { return m_viewMode; }
    QString currentFolder() const { return m_currentFolder; }
    QString currentFolderName() const;
    QVariantList breadcrumb() const { return m_breadcrumb; }
    int folderCount() const { return m_folderCount; }

    int visibleCount() const;
    QStringList selectedTags() const;
    QStringList availableTags() const;
    QString searchText() const;

    QVariantMap currentItem() const { return m_currentItem; }
    QString playbackError() const;

    // QML drives these directly (search field, view switch, folder pickers), so
    // they have to be invokable: being a Q_PROPERTY WRITE accessor is not enough.
    Q_INVOKABLE void setLibraryRoot(const QString &path);
    Q_INVOKABLE void setViewMode(const QString &mode);
    Q_INVOKABLE void setSelectedTags(const QStringList &tags);
    Q_INVOKABLE void setSearchText(const QString &text);

public slots:
    /** Called from QML once the application window exists. */
    void attachWindow(QObject *window);

    void rescan();
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

    void setToolDirectory(const QString &dir);
    void setPlayMode(const QString &mode);
    void setThemeMode(const QString &mode);
    void clearThumbnailCache();
    void openWithSystemPlayer(const QString &filePath);
    void revealInFileManager(const QString &filePath);

    /** Returns an empty string when the directory is usable, an error otherwise. */
    Q_INVOKABLE QString checkToolDirectory(const QString &dir) const;
    Q_INVOKABLE QVariantMap itemAt(int row) const;
    Q_INVOKABLE QString formatDuration(double seconds) const;

    /** Cycles standalone -> embedded -> system -> standalone. */
    Q_INVOKABLE QString nextPlayMode() const;
    /** Human readable label for a play mode key. */
    Q_INVOKABLE QString playModeLabel(const QString &mode) const;

    /** Converts a `file:///...` url coming from a dialog into a local path. */
    Q_INVOKABLE QString toLocalPath(const QString &urlOrPath) const;

signals:
    void playerChanged();
    void libraryRootChanged();
    void scanningChanged();
    void progressChanged();
    void statusMessageChanged();
    void errorMessageChanged();
    void viewModeChanged();
    void currentFolderChanged();
    void breadcrumbChanged();
    void folderCountChanged();
    void visibleCountChanged();
    void selectedTagsChanged();
    void availableTagsChanged();
    void searchTextChanged();
    void currentItemChanged();
    void thumbnailReady(const QString &filePath);
    void playbackRequested(const QString &filePath);

private:
    void wireSettings();
    void startScan();
    void rebuildPlayer();
    /** Shows the ffmpeg/ffplay error only when the active backend needs them. */
    void refreshToolError();
    /** The active backend, or null when playback is owned by another app. */
    PlayerController *controllablePlayer() const;
    void rebuildBreadcrumb();
    void updateCurrentFolderModel();
    void setStatus(const QString &message);
    void setError(const QString &message);

    void onItemsReady(const MediaItemList &items);
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

    QQuickWindow *m_window = nullptr;
    quintptr m_windowHandle = 0;
    QRectF m_videoRectDip;

    QHash<QString, FolderNode> m_folderNodes;
    QHash<QString, QString> m_folderCoverHints;

    QString m_viewMode = QStringLiteral("flat");
    QString m_currentFolder;
    QVariantList m_breadcrumb;
    QVariantMap m_currentItem;
    int m_folderCount = 0;
    bool m_scanning = false;
    int m_scannedFiles = 0;
    int m_scannedVideos = 0;
    QString m_statusMessage;
    QString m_errorMessage;
};
