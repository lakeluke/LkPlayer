#pragma once

#include <atomic>

#include <QCache>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QtQml/qqmlregistration.h>

class ThumbnailCache;

/** Per-item metadata used to generate a cover without touching the disk. */
struct ThumbnailMeta
{
    QString posterPath;
    qint64 fileSize = -1;
    qint64 lastModified = -1;
};

/**
 * Produces cover images for videos.
 *
 * Covers come from two sources: a sidecar poster file (read once and cached)
 * or a frame grabbed with ffmpeg. Both are generated explicitly through
 * requestMissingGeneration() (driven by the "generate covers" action) rather
 * than on demand, so browsing never touches the media drive.
 *
 * `lookup()` is called from the Qt Quick image loading threads and only ever
 * consults the in-memory cache and the SQLite cache; it never touches the
 * media files or the video files themselves.
 */
class ThumbnailService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY pendingCountChanged)

public:
    explicit ThumbnailService(ThumbnailCache *cache, QObject *parent = nullptr);
    ~ThumbnailService() override;

    void setFfmpegPath(const QString &path);
    QString ffmpegPath() const { return m_ffmpegPath; }

    /** Toggles cover generation. When disabled, only cached covers remain. */
    void setGenerationEnabled(bool enabled);
    bool generationEnabled() const { return m_generationEnabled.load(std::memory_order_relaxed); }

    ThumbnailCache *cache() const { return m_cache; }

    /** Registers metadata for the current library (GUI thread). */
    void setItemMetadata(const QString &filePath, const ThumbnailMeta &meta);
    void clearItemMetadata();

    /** Thread safe. Returns the best cached image available right now. */
    QImage lookup(const QString &videoPath) const;

    /**
     * Thread safe. Queues generation for every path whose cover is missing;
     * poster files are copied and cached immediately on the calling thread,
     * videos are extracted by ffmpeg on the worker pool. Returns the number
     * scheduled.
     */
    int requestMissingGeneration(const QStringList &videoPaths);

    /** Thread safe. Called from the worker pool when ffmpeg finished. */
    void completeGeneration(const QString &videoPath, qint64 fileSize, qint64 lastModified, const QImage &image);

    int pendingCount() const;

    Q_INVOKABLE qint64 cacheBytes() const;
    Q_INVOKABLE int cacheCount() const;
    Q_INVOKABLE int clearCache();

signals:
    void thumbnailReady(const QString &videoPath);
    void pendingCountChanged();

private:
    ThumbnailCache *m_cache = nullptr;
    QString m_ffmpegPath;
    QThreadPool m_pool;

    mutable QMutex m_mutex;
    mutable QCache<QString, QImage> m_memory;
    mutable QSet<QString> m_pending;
    mutable QSet<QString> m_failed;
    QHash<QString, ThumbnailMeta> m_meta;
    QImage m_stub;
    std::atomic<bool> m_generationEnabled{true};
};
