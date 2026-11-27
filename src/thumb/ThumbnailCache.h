#pragma once

#include <QImage>
#include <QObject>
#include <QString>

class QSqlDatabase;

/**
 * SQLite-backed cache for generated covers (ffmpeg frame grabs).
 *
 * Covers are stored as BLOBs in a single database file rather than one JPG per
 * entry: reading a cover becomes a single indexed lookup instead of opening
 * thousands of scattered files, which matters a lot on mechanical disks.
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

    /**
     * Cache key derived from the video path, size and modification time. The
     * size/mtime normally come from the scan index so a lookup never has to
     * stat the video file on disk; pass -1 for either to fall back to stat.
     */
    QString keyFor(const QString &videoPath, qint64 fileSize = -1, qint64 lastModified = -1) const;
    /** Temporary file ffmpeg writes into before the image is read back. */
    QString tempFileFor(const QString &videoPath, qint64 fileSize = -1, qint64 lastModified = -1) const;

    bool contains(const QString &videoPath, qint64 fileSize = -1, qint64 lastModified = -1) const;
    QImage load(const QString &videoPath, qint64 fileSize = -1, qint64 lastModified = -1) const;

    /** Moves a freshly generated image into the cache. */
    bool store(const QString &videoPath, qint64 fileSize, qint64 lastModified, const QImage &image);

    qint64 totalBytes() const;
    int entryCount() const;

    /** Removes the least recently used entries until both limits are met. */
    void prune(qint64 maxBytes = 512LL * 1024 * 1024, int maxEntries = 20000);

    /** Deletes every cached cover and returns the number of removed rows. */
    int clear();

signals:
    void changed();

private:
    /** Lazily returns a connection bound to the calling thread. */
    QSqlDatabase connection() const;
    void ensureSchema(QSqlDatabase &db) const;

    QString m_dbPath;
};
