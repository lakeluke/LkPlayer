#include "thumb/ThumbnailProvider.h"

#include "core/PathUtils.h"
#include "thumb/ThumbnailService.h"

ThumbnailProvider::ThumbnailProvider(ThumbnailService *service)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_service(service)
{
}

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    if (size)
        *size = QSize();

    if (!m_service)
        return QImage();

    QString rawId = id;
    const int marker = rawId.indexOf(QLatin1Char('?'));
    if (marker >= 0)
        rawId = rawId.left(marker);

    const QString videoPath = PathUtils::decodeId(rawId);
    if (videoPath.isEmpty())
        return QImage();

    bool needsGeneration = false;
    QImage image = m_service->lookup(videoPath, &needsGeneration);
    if (needsGeneration)
        m_service->requestGeneration(videoPath);

    if (size)
        *size = image.size();

    if (!requestedSize.isEmpty() && !image.isNull()
        && (image.width() > requestedSize.width() || image.height() > requestedSize.height())) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return image;
}
