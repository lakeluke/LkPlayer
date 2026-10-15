#pragma once

#include <QAtomicInt>
#include <QFuture>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QString>

#include "core/FolderNode.h"
#include "core/MediaItem.h"

/**
 * Recursively walks a media library root and reports videos in batches so the
 * UI can stay responsive and fill up progressively.
 *
 * The heavy lifting happens inside a worker thread started through
 * QtConcurrent; all signals are delivered to the thread that owns the scanner
 * (the GUI thread).
 */
class LibraryScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString rootPath READ rootPath WRITE setRootPath NOTIFY rootPathChanged)

public:
    explicit LibraryScanner(QObject *parent = nullptr);
    ~LibraryScanner() override;

    bool isRunning() const { return m_running; }
    QString rootPath() const { return m_root; }
    void setRootPath(const QString &path);

    /** Videos discovered by the most recent scan (read only after scanFinished). */
    int lastVideoCount() const { return m_videoCount; }

public slots:
    void start();
    void cancel();

signals:
    void runningChanged(bool running);
    void rootPathChanged(const QString &path);
    void scanStarted();
    void itemsReady(const MediaItemList &items);
    void structureReady(const FolderNodeList &folders);
    void progressChanged(int scannedFiles, int videosFound, const QString &currentFolder);
    void scanFinished(int totalVideos, bool cancelled);
    void errorOccurred(const QString &message);

private:
    void runScan();
    void scanDirectory(const QString &dirPath, int depth);
    void flushItems(bool force);
    void emitProgress(bool force);

    QString m_root;
    QAtomicInt m_cancelFlag{0};
    QAtomicInt m_runningFlag{0};
    bool m_running = false;
    QFuture<void> m_future;
    QFutureWatcher<void> m_watcher;

    // --- state owned by the worker -----------------------------------------
    QString m_rootAbsolute;
    MediaItemList m_pendingItems;
    QHash<QString, FolderNode> m_folders;
    int m_scannedFiles = 0;
    int m_videoCount = 0;
    qint64 m_lastEmitMs = 0;
    qint64 m_lastProgressMs = 0;
};
