#pragma once

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>
#include <QtQml/qqmlregistration.h>

class ThumbnailCache;

/**
 * Produces cover images for videos that have no sidecar poster.
 *
 * `lookup()` is called from the Qt Quick image loading threads, so it is
 * deliberately thread safe: it returns immediately (a transparent stub when
 * nothing is ready yet) and schedules ffmpeg extraction in the background.
 * Once a cover is available `thumbnailReady()` is emitted on the service
 * thread so the UI can re-request the image.
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

    ThumbnailCache *cache() const { return m_cache; }

    /** Thread safe. Returns the best image available right now. */
    QImage lookup(const QString &videoPath, bool *needsGeneration) const;

    /** Thread safe. Schedules extraction unless it is already pending/done. */
    void requestGeneration(const QString &videoPath);

    /** Thread safe. Called from the worker pool when ffmpeg finished. */
    void completeGeneration(const QString &videoPath, const QImage &image);

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
    QImage m_stub;
};
