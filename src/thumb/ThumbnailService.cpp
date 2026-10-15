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
    ThumbnailJob(ThumbnailService *service, QString videoPath, QString ffmpegPath, QString outputPath)
        : m_service(service)
        , m_videoPath(std::move(videoPath))
        , m_ffmpegPath(std::move(ffmpegPath))
        , m_outputPath(std::move(outputPath))
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
            [service = m_service, path = m_videoPath, image]() { service->completeGeneration(path, image); },
            Qt::QueuedConnection);
    }

private:
    ThumbnailService *m_service;
    QString m_videoPath;
    QString m_ffmpegPath;
    QString m_outputPath;
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

int ThumbnailService::pendingCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_pending.size();
}

QImage ThumbnailService::lookup(const QString &videoPath, bool *needsGeneration) const
{
    if (needsGeneration)
        *needsGeneration = false;

    if (videoPath.isEmpty() || !m_cache)
        return m_stub;

    {
        QMutexLocker locker(&m_mutex);
        if (QImage *cached = m_memory.object(videoPath))
            return *cached;
        if (m_failed.contains(videoPath))
            return m_stub;
    }

    if (m_cache->contains(videoPath)) {
        const QImage image = m_cache->load(videoPath);
        if (!image.isNull()) {
            QMutexLocker locker(&m_mutex);
            m_memory.insert(videoPath, new QImage(image), 1);
            return image;
        }
    }

    {
        QMutexLocker locker(&m_mutex);
        if (m_pending.contains(videoPath))
            return m_stub;
        m_pending.insert(videoPath);
    }

    if (needsGeneration)
        *needsGeneration = true;
    return m_stub;
}

void ThumbnailService::requestGeneration(const QString &videoPath)
{
    if (videoPath.isEmpty())
        return;

    if (m_ffmpegPath.isEmpty() || !QFileInfo::exists(m_ffmpegPath)) {
        qCDebug(lcThumb) << "no usable ffmpeg, skipping cover for" << videoPath;
        QMutexLocker locker(&m_mutex);
        m_pending.remove(videoPath);
        m_failed.insert(videoPath);
        return;
    }

    const QString outputPath = m_cache->pathFor(videoPath) + QStringLiteral(".part.jpg");
    m_pool.start(new ThumbnailJob(this, videoPath, m_ffmpegPath, outputPath));
}

void ThumbnailService::completeGeneration(const QString &videoPath, const QImage &image)
{
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
        m_cache->store(videoPath, image);
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
