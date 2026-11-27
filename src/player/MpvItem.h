#pragma once

#include <QPointer>
#include <QQuickFramebufferObject>
#include <QSharedPointer>

#include "player/PlayerController.h"

struct mpv_handle;
struct MpvUpdateBridge;

class QTimer;

/**
 * QML item that lets libmpv paint into the Qt Quick scene graph.
 *
 * The frames are pulled on Qt's render thread through libmpv's OpenGL render
 * API, so the video is a first-class part of the same window as the rest of the
 * UI: one swap chain, one vsync, no cross-process window juggling.
 *
 * This requires Qt's scene graph to run on OpenGL (`QQuickWindow::setGraphicsApi`
 * in main.cpp) - libmpv has no Direct3D render backend.
 */
class MpvItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(PlayerController *player READ player WRITE setPlayer NOTIFY playerChanged)
    /** Non-empty when mpv could not set up its OpenGL render context. */
    Q_PROPERTY(QString renderError READ renderError NOTIFY renderErrorChanged)

public:
    explicit MpvItem(QQuickItem *parent = nullptr);
    ~MpvItem() override;

    PlayerController *player() const { return m_player.data(); }
    void setPlayer(PlayerController *player);

    QString renderError() const { return m_renderError; }
    void setRenderError(const QString &message);

    QQuickFramebufferObject::Renderer *createRenderer() const override;

    /** Libmpv handle to render; read by the renderer on the render thread. */
    mpv_handle *mpvHandle() const;
    /** Shared guard used by libmpv's cross-thread update callback. */
    QSharedPointer<MpvUpdateBridge> bridge() const { return m_bridge; }

    /** Queued by libmpv's update callback to wake the scene graph up. */
    void scheduleUpdate();

    /**
     * Queued from the render thread once mpv's OpenGL context is live; tells
     * the engine that loading files will now produce frames.
     */
    void handleRenderReady();

    /**
     * Queued from the render thread when the context is torn down (for example
     * after the backend switched away from embedded). Resets the ready flag and
     * tells the engine it must wait for a new context before loading again.
     */
    void handleRenderContextLost();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

signals:
    void playerChanged();
    void renderErrorChanged();

private:
    QPointer<PlayerController> m_player;
    QSharedPointer<MpvUpdateBridge> m_bridge;
    QString m_renderError;
    bool m_renderReady = false;
    /** Debounces cover-fit updates while the panel fold animation runs. */
    QTimer *m_fitTimer = nullptr;
};
