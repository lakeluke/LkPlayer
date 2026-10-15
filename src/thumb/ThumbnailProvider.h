#pragma once

#include <QQuickImageProvider>

class ThumbnailService;

/**
 * Serves `image://thumb/<base64url(video path)>` requests. The identifier may
 * carry a `?v=<n>` cache buster which is stripped before decoding.
 */
class ThumbnailProvider : public QQuickImageProvider
{
public:
    explicit ThumbnailProvider(ThumbnailService *service);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    ThumbnailService *m_service;
};
