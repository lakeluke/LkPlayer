#pragma once

#include <QImage>
#include <QObject>
#include <QString>

/**
 * On-disk cache for generated covers (ffmpeg frame grabs).
 *
 * The cache key is a hash of the video path together with its size and
 * modification time, so a re-encoded file automatically invalidates its
 * cached cover.
 */
class ThumbnailCache : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailCache(QObject *parent = nullptr);

    QString directory() const { return m_directory; }

    QString keyFor(const QString &videoPath) const;
    QString pathFor(const QString &videoPath) const;

    bool contains(const QString &videoPath) const;
    QImage load(const QString &videoPath) const;

    /** Moves a freshly generated image into the cache. */
    bool store(const QString &videoPath, const QImage &image);

    qint64 totalBytes() const;
    int entryCount() const;

    /** Removes the least recently used entries until both limits are met. */
    void prune(qint64 maxBytes = 512LL * 1024 * 1024, int maxEntries = 20000);

    /** Deletes every cached cover and returns the number of removed files. */
    int clear();

signals:
    void changed();

private:
    QString m_directory;
};
