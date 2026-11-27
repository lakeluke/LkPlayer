#include "thumb/ThumbnailService.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QMutexLocker>
#include <QProcess>
#include <QRunnable>
#include <QThread>

#include "thumb/ThumbnailCache.h"

Q_LOGGING_CATEGORY(lcThumb, "lk.thumb")

namespace {

// Covers are normalised to a 2:3 portrait card so the grid stays uniform
// whether the artwork comes from a poster or from a video frame.
constexpr int kCoverWidth = 480;
constexpr int kCoverHeight = 720;
constexpr int kProcessTimeoutMs = 30000;

/** Timestamps tried in order; the first successful grab wins. */
const double kSeekCandidates[] = { 15.0, 5.0, 1.0, 0.0 };

QImage normalizeCover(const QImage &source)
{
    const QImage scaled = source.scaled(kCoverWidth, kCoverHeight,
                                        Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int x = (scaled.width() - kCoverWidth) / 2;
    const int y = (scaled.height() - kCoverHeight) / 2;
    return scaled.copy(x, y, kCoverWidth, kCoverHeight);
}

QImage grabFrame(const QString &ffmpegPath, const QString &videoPath, double seekSeconds, const QString &outputPath)
{
    QProcess process;
    process.setProgram(ffmpegPath);
    process.setArguments({
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-nostdin"),
        QStringLiteral("-y"),
        QStringLiteral("-ss"), QString::number(seekSeconds, 'f', 3),
        QStringLiteral("-i"), videoPath,
        QStringLiteral("-an"),
        QStringLiteral("-sn"),
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-vf"),
        QStringLiteral("scale=%1:%2:force_original_aspect_ratio=increase,crop=%1:%2")
            .arg(kCoverWidth)
            .arg(kCoverHeight),
        QStringLiteral("-q:v"), QStringLiteral("4"),
        outputPath,
    });

    QFile::remove(outputPath);
    process.start();
    if (!process.waitForStarted(5000))
        return QImage();
    if (!process.waitForFinished(kProcessTimeoutMs)) {
        process.kill();
        process.waitForFinished(2000);
        return QImage();
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return QImage();

    QImage image;
    if (!image.load(outputPath))
        return QImage();
    return image;
}

/** Runs one ffmpeg extraction on a pool thread. */
class ThumbnailJob : public QRunnable
{
public:
    ThumbnailJob(ThumbnailService *service, QString videoPath, QString ffmpegPath, QString outputPath,
                 qint64 fileSize, qint64 lastModified)
        : m_service(service)
        , m_videoPath(std::move(videoPath))
        , m_ffmpegPath(std::move(ffmpegPath))
        , m_outputPath(std::move(outputPath))
        , m_fileSize(fileSize)
        , m_lastModified(lastModified)
    {
    }

    void run() override
    {
        QImage image;
        for (double seek : kSeekCandidates) {
            image = grabFrame(m_ffmpegPath, m_videoPath, seek, m_outputPath);
            if (!image.isNull())
                break;
        }

        QFile::remove(m_outputPath);

        // Deliver on the service thread.
        QMetaObject::invokeMethod(
            m_service,
            [service = m_service, path = m_videoPath, image, fileSize = m_fileSize, lastModified = m_lastModified]() {
                service->completeGeneration(path, fileSize, lastModified, image);
            },
            Qt::QueuedConnection);
    }

private:
    ThumbnailService *m_service;
    QString m_videoPath;
    QString m_ffmpegPath;
    QString m_outputPath;
    qint64 m_fileSize;
    qint64 m_lastModified;
};

} // namespace

ThumbnailService::ThumbnailService(ThumbnailCache *cache, QObject *parent)
    : QObject(parent)
    , m_cache(cache)
{
    m_memory.setMaxCost(256);

    QImage stub(4, 6, QImage::Format_ARGB32_Premultiplied);
    stub.fill(Qt::transparent);
    m_stub = stub;

    m_pool.setMaxThreadCount(qBound(1, QThread::idealThreadCount() / 2, 3));
    m_pool.setExpiryTimeout(15000);
}

ThumbnailService::~ThumbnailService()
{
    m_pool.clear();
    m_pool.waitForDone(3000);
}

void ThumbnailService::setFfmpegPath(const QString &path)
{
    if (path == m_ffmpegPath)
        return;
    m_ffmpegPath = path;

    // A new tool directory may fix previously failed extractions.
    QMutexLocker locker(&m_mutex);
    m_failed.clear();
}

void ThumbnailService::setGenerationEnabled(bool enabled)
{
    m_generationEnabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        QMutexLocker locker(&m_mutex);
        m_pending.clear();
    }
    emit pendingCountChanged();
}

int ThumbnailService::pendingCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_pending.size();
}

void ThumbnailService::setItemMetadata(const QString &filePath, const ThumbnailMeta &meta)
{
    if (filePath.isEmpty())
        return;
    QMutexLocker locker(&m_mutex);
    m_meta.insert(filePath, meta);
}

void ThumbnailService::clearItemMetadata()
{
    QMutexLocker locker(&m_mutex);
    m_meta.clear();
    m_memory.clear();
    m_failed.clear();
}

QImage ThumbnailService::lookup(const QString &videoPath) const
{
    if (videoPath.isEmpty() || !m_cache)
        return m_stub;

    {
        QMutexLocker locker(&m_mutex);
        if (QImage *cached = m_memory.object(videoPath))
            return *cached;
        if (m_failed.contains(videoPath))
            return m_stub;
    }

    qint64 fileSize = -1;
    qint64 lastModified = -1;
    {
        QMutexLocker locker(&m_mutex);
        const auto it = m_meta.constFind(videoPath);
        if (it != m_meta.constEnd()) {
            fileSize = it->fileSize;
            lastModified = it->lastModified;
        }
    }

    if (m_cache->contains(videoPath, fileSize, lastModified)) {
        const QImage image = m_cache->load(videoPath, fileSize, lastModified);
        if (!image.isNull()) {
            QMutexLocker locker(&m_mutex);
            m_memory.insert(videoPath, new QImage(image), 1);
            return image;
        }
    }

    return m_stub;
}

int ThumbnailService::requestMissingGeneration(const QStringList &videoPaths)
{
    if (!m_generationEnabled.load(std::memory_order_relaxed))
        return 0;

    int scheduled = 0;
    for (const QString &path : videoPaths) {
        if (path.isEmpty())
            continue;

        ThumbnailMeta meta;
        bool haveMeta = false;
        {
            QMutexLocker locker(&m_mutex);
            if (m_pending.contains(path) || m_failed.contains(path) || m_memory.contains(path))
                continue;
            const auto it = m_meta.constFind(path);
            if (it == m_meta.constEnd())
                continue;
            meta = it.value();
            haveMeta = true;
        }
        if (!haveMeta)
            continue;

        if (m_cache && m_cache->contains(path, meta.fileSize, meta.lastModified))
            continue;

        // Sidecar poster: read and cache it directly, no ffmpeg needed.
        if (!meta.posterPath.isEmpty()) {
            const QImage poster(meta.posterPath);
            if (poster.isNull()) {
                QMutexLocker locker(&m_mutex);
                m_failed.insert(path);
                continue;
            }
            const QImage cover = normalizeCover(poster);
            if (m_cache && m_cache->store(path, meta.fileSize, meta.lastModified, cover)) {
                QMutexLocker locker(&m_mutex);
                m_memory.insert(path, new QImage(cover), 1);
                ++scheduled;
            }
            continue;
        }

        // No poster: grab a frame with ffmpeg on the worker pool.
        if (m_ffmpegPath.isEmpty() || !QFileInfo::exists(m_ffmpegPath)) {
            QMutexLocker locker(&m_mutex);
            m_failed.insert(path);
            continue;
        }

        {
            QMutexLocker locker(&m_mutex);
            m_pending.insert(path);
        }
        const QString outputPath = m_cache->tempFileFor(path, meta.fileSize, meta.lastModified);
        m_pool.start(new ThumbnailJob(this, path, m_ffmpegPath, outputPath, meta.fileSize, meta.lastModified));
        ++scheduled;
    }

    if (scheduled > 0)
        emit pendingCountChanged();
    return scheduled;
}

void ThumbnailService::completeGeneration(const QString &videoPath, qint64 fileSize, qint64 lastModified, const QImage &image)
{
    // The user switched extraction off while this job was running: drop the
    // result instead of writing a cover they asked not to generate.
    if (!m_generationEnabled.load(std::memory_order_relaxed)) {
        QMutexLocker locker(&m_mutex);
        m_pending.remove(videoPath);
        emit pendingCountChanged();
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_pending.remove(videoPath);
        if (image.isNull()) {
            m_failed.insert(videoPath);
        } else {
            m_memory.insert(videoPath, new QImage(image), 1);
        }
    }

    if (!image.isNull() && m_cache)
        m_cache->store(videoPath, fileSize, lastModified, image);
    else
        qCDebug(lcThumb) << "no frame extracted for" << videoPath;

    emit pendingCountChanged();
    emit thumbnailReady(videoPath);
}

qint64 ThumbnailService::cacheBytes() const
{
    return m_cache ? m_cache->totalBytes() : 0;
}

int ThumbnailService::cacheCount() const
{
    return m_cache ? m_cache->entryCount() : 0;
}

int ThumbnailService::clearCache()
{
    const int removed = m_cache ? m_cache->clear() : 0;
    {
        QMutexLocker locker(&m_mutex);
        m_memory.clear();
        m_failed.clear();
    }
    emit pendingCountChanged();
    return removed;
}
