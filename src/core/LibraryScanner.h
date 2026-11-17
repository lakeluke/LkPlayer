#pragma once

#include <QAtomicInt>
#include <QFuture>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QString>

#include "core/FolderNode.h"
#include "core/LibraryIndex.h"
#include "core/MediaItem.h"

/**
 * Recursively walks a media library root and reports videos in batches so the
 * UI can stay responsive and fill up progressively.
 *
 * The heavy lifting happens inside a worker thread started through
 * QtConcurrent; all signals are delivered to the thread that owns the scanner
 * (the GUI thread).
 *
 * Walking a big library on a mechanical disk is slow, so the result of the last
 * walk is kept in a `LibraryIndex`: an unchanged directory (same modification
 * time) is taken straight from the index, only directories that actually
 * changed are read again.
 */
class LibraryScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString rootPath READ rootPath WRITE setRootPath NOTIFY rootPathChanged)

public:
    /** How a scan uses the cached index. */
    enum class Mode {
        Snapshot, //!< publish the cached index first, then verify it
        Verify,   //!< background check: only reports the directories that changed
        Full,     //!< ignore the cache and read every directory again
    };

    explicit LibraryScanner(QObject *parent = nullptr);
    ~LibraryScanner() override;

    bool isRunning() const { return m_running; }
    QString rootPath() const { return m_root; }
    void setRootPath(const QString &path);

    /**
     * Where the scan index is cached. Empty disables caching entirely, which
     * keeps this class usable without the application path helpers.
     */
    void setIndexFile(const QString &filePath) { m_indexFile = filePath; }
    QString indexFile() const { return m_indexFile; }

    Mode mode() const { return m_mode; }
    /** True for background checks, which must not disturb the visible state. */
    bool isBackground() const { return m_mode == Mode::Verify; }

public slots:
    /** Cached snapshot first (when there is one), then an incremental verify. */
    void start();
    void cancel();

public:
    void startWithMode(Mode mode);

signals:
    void runningChanged(bool running);
    void rootPathChanged(const QString &path);
    void scanStarted();
    /** Videos of newly discovered folders, used by fresh scans. */
    void itemsReady(const MediaItemList &items);
    /** Videos of a re-read folder; replaces whatever the model had for it. */
    void folderItemsChanged(const QString &folderPath, const MediaItemList &items);
    void structureReady(const FolderNodeList &folders);
    void progressChanged(int scannedFiles, int videosFound, const QString &currentFolder);
    /** Emitted after `itemsReady` when the cached snapshot was published. */
    void indexLoaded(int videos, int folders);
    void scanFinished(int totalVideos, bool cancelled);
    void errorOccurred(const QString &message);

private:
    void beginScan(Mode mode);
    void runScan();
    void scanDirectory(const QString &dirPath, int depth);
    /** Merges the child video counts and tags into the folder node. */
    void rollUpFolder(const QString &absPath);
    FolderNodeList folderList() const;
    void flushItems(bool force);
    void emitProgress(bool force);

    QString m_root;
    QString m_indexFile;
    QAtomicInt m_cancelFlag{0};
    bool m_running = false;
    QFuture<void> m_future;
    QFutureWatcher<void> m_watcher;
    Mode m_mode = Mode::Snapshot;

    // --- state owned by the worker -----------------------------------------
    QString m_rootAbsolute;
    MediaItemList m_pendingItems;
    QHash<QString, FolderNode> m_folders;
    LibraryIndex m_index;
    bool m_haveIndex = false;
    /** True when the model already holds the cached rows: changes are deltas. */
    bool m_deltaMode = false;
    bool m_indexDirty = false;
    int m_scannedFiles = 0;
    int m_videoCount = 0;
    qint64 m_lastEmitMs = 0;
    qint64 m_lastProgressMs = 0;
};
