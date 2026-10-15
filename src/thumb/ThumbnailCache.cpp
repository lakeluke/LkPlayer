#include "thumb/ThumbnailCache.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcThumbCache, "lk.thumb.cache")

namespace {
const QString kExtension = QStringLiteral("jpg");
}

ThumbnailCache::ThumbnailCache(QObject *parent)
    : QObject(parent)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    m_directory = QDir(base).absoluteFilePath(QStringLiteral("covers"));
    if (!QDir().mkpath(m_directory))
        qCWarning(lcThumbCache) << "could not create thumbnail cache directory" << m_directory;
}

QString ThumbnailCache::keyFor(const QString &videoPath) const
{
    const QFileInfo info(videoPath);
    const QString identity = QStringLiteral("%1|%2|%3")
                                 .arg(info.absoluteFilePath())
                                 .arg(info.size())
                                 .arg(info.lastModified().toMSecsSinceEpoch());
    const QByteArray digest = QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha1);
    return QString::fromLatin1(digest.toHex());
}

QString ThumbnailCache::pathFor(const QString &videoPath) const
{
    return QDir(m_directory).absoluteFilePath(keyFor(videoPath) + QLatin1Char('.') + kExtension);
}

bool ThumbnailCache::contains(const QString &videoPath) const
{
    if (videoPath.isEmpty())
        return false;
    return QFileInfo::exists(pathFor(videoPath));
}

QImage ThumbnailCache::load(const QString &videoPath) const
{
    if (!contains(videoPath))
        return QImage();

    QImage image;
    if (!image.load(pathFor(videoPath)))
        return QImage();

    // Touch the file so the LRU pruning keeps frequently used covers.
    QFile(pathFor(videoPath)).setFileTime(QDateTime::currentDateTime(), QFileDevice::FileAccessTime);
    return image;
}

bool ThumbnailCache::store(const QString &videoPath, const QImage &image)
{
    if (videoPath.isEmpty() || image.isNull())
        return false;

    const QString target = pathFor(videoPath);
    const QString temp = target + QStringLiteral(".part");

    if (!image.save(temp, "JPG", 88)) {
        qCWarning(lcThumbCache) << "failed to write thumbnail" << temp;
        QFile::remove(temp);
        return false;
    }

    QFile::remove(target);
    if (!QFile::rename(temp, target)) {
        qCWarning(lcThumbCache) << "failed to move thumbnail into place" << target;
        QFile::remove(temp);
        return false;
    }

    emit changed();
    return true;
}

qint64 ThumbnailCache::totalBytes() const
{
    qint64 total = 0;
    const QDir dir(m_directory);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::NoSort);
    for (const QFileInfo &info : entries)
        total += info.size();
    return total;
}

int ThumbnailCache::entryCount() const
{
    return QDir(m_directory).entryList(QDir::Files | QDir::Readable, QDir::NoSort).size();
}

void ThumbnailCache::prune(qint64 maxBytes, int maxEntries)
{
    QDir dir(m_directory);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Time | QDir::Reversed);

    qint64 total = 0;
    int count = entries.size();
    for (const QFileInfo &info : entries)
        total += info.size();

    if (total <= maxBytes && count <= maxEntries)
        return;

    int removed = 0;
    for (const QFileInfo &info : entries) {
        if (total <= maxBytes && count <= maxEntries)
            break;
        const qint64 size = info.size();
        if (QFile::remove(info.absoluteFilePath())) {
            total -= size;
            --count;
            ++removed;
        }
    }
    qCInfo(lcThumbCache) << "pruned" << removed << "cached covers";
}

int ThumbnailCache::clear()
{
    QDir dir(m_directory);
    const QStringList files = dir.entryList(QDir::Files, QDir::NoSort);
    int removed = 0;
    for (const QString &file : files) {
        if (QFile::remove(dir.absoluteFilePath(file)))
            ++removed;
    }
    if (removed > 0)
        emit changed();
    qCInfo(lcThumbCache) << "cleared" << removed << "cached covers";
    return removed;
}
