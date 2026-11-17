#include "player/MpvItem.h"

#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

#include "player/MpvEngine.h"
#include "player/MpvPlayer.h"

Q_LOGGING_CATEGORY(lcMpvItem, "lk.player.mpv.item")

/**
 * Bridges libmpv's update callback (called on an mpv thread) and the QQuickItem
 * (GUI thread). The item nulls the pointer under the mutex before it dies, so
 * the callback can never reach a dangling item.
 */
struct MpvUpdateBridge
{
    QMutex mutex;
    MpvItem *item = nullptr;
};

namespace {

void onRenderUpdate(void *ctx)
{
    auto *bridge = static_cast<MpvUpdateBridge *>(ctx);
    QMutexLocker locker(&bridge->mutex);
    if (!bridge->item)
        return;
    // A queued call is safe even if the item disappears before it is delivered:
    // Qt drops pending meta-calls for a deleted receiver.
    QMetaObject::invokeMethod(bridge->item, &MpvItem::scheduleUpdate, Qt::QueuedConnection);
}

void *resolveGlProc(void *ctx, const char *name)
{
    auto *context = static_cast<QOpenGLContext *>(ctx);
    return context ? reinterpret_cast<void *>(context->getProcAddress(name)) : nullptr;
}

} // namespace

/**
 * Runs on Qt's render thread with the scene graph's OpenGL context current.
 * Everything below mirrors mpv's own `libmpv` examples: create the context
 * lazily on the first frame, render into the FBO Qt already bound, and report
 * the swap so mpv can pace itself against the display.
 */
class MpvRenderer : public QQuickFramebufferObject::Renderer
{
public:
    MpvRenderer() = default;
    ~MpvRenderer() override { destroyContext(); }

    void synchronize(QQuickFramebufferObject *item) override
    {
        auto *mpvItem = static_cast<MpvItem *>(item);
        const auto handle = mpvItem->mpvHandle();
        if (handle != m_handle) {
            // A new engine means a new handle: retry whatever failed before.
            m_initFailed = false;
            m_handle = handle;
        }
        m_bridge = mpvItem->bridge();

        const qreal dpr = item->window() ? item->window()->effectiveDevicePixelRatio() : 1.0;
        m_size = QSize(qRound(item->width() * dpr), qRound(item->height() * dpr));
    }

    void render() override
    {
        if (m_handle != m_contextHandle && !m_initFailed) {
            destroyContext();
            if (m_handle)
                ensureContext();
        }

        if (!m_context || m_size.isEmpty())
            return;

        QOpenGLFunctions *gl = m_gl ? m_gl->functions() : nullptr;
        if (!gl) {
            qCWarning(lcMpvItem) << "no OpenGL functions available in the render thread";
            return;
        }

        // Qt has already bound the framebuffer object it composites from. mpv
        // must be given that very id: in mpv_opengl_fbo, 0 means the *default*
        // framebuffer (the window), not "whatever is bound right now", so
        // passing 0 renders the video outside Qt's FBO and the player stays
        // black.
        GLint boundFbo = 0;
        gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFbo);

        mpv_opengl_fbo fbo{};
        fbo.fbo = static_cast<int>(boundFbo);
        fbo.w = m_size.width();
        fbo.h = m_size.height();

        // Qt samples this FBO with the same bottom-left origin OpenGL uses, so
        // mpv must write the frame unflipped: MPV_RENDER_PARAM_FLIP_Y = 1 would
        // mirror the picture vertically on screen. Verified against a captured
        // window, not against QQuickWindow::grabWindow() - the latter flips the
        // image again on read-back and hides exactly this mistake.
        int flipY = 0;
        mpv_render_param params[] = {
            { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
            { MPV_RENDER_PARAM_FLIP_Y, &flipY },
            { MPV_RENDER_PARAM_INVALID, nullptr },
        };

        mpv_render_context_render(m_context, params);
        mpv_render_context_report_swap(m_context);
    }

private:
    /** Surfaces a render-thread failure on the item, where QML can show it. */
    void reportError(const QString &message)
    {
        m_initFailed = true;
        if (!m_bridge)
            return;

        QMutexLocker locker(&m_bridge->mutex);
        MpvItem *item = m_bridge->item;
        if (!item)
            return;
        QMetaObject::invokeMethod(
            item, [item, message]() { item->setRenderError(message); }, Qt::QueuedConnection);
    }

    /** Hands the "context is live" fact over to the GUI thread. */
    void notifyReady()
    {
        if (!m_bridge)
            return;

        QMutexLocker locker(&m_bridge->mutex);
        MpvItem *item = m_bridge->item;
        if (!item)
            return;
        QMetaObject::invokeMethod(item, &MpvItem::handleRenderReady, Qt::QueuedConnection);
    }

    void ensureContext()
    {
        QOpenGLContext *gl = QOpenGLContext::currentContext();
        if (!gl) {
            qCWarning(lcMpvItem) << "no current OpenGL context; cannot set up mpv rendering";
            reportError(QStringLiteral("无法获取 OpenGL 渲染上下文，内嵌播放不可用"));
            return;
        }

        mpv_opengl_init_params initParams{};
        initParams.get_proc_address = &resolveGlProc;
        initParams.get_proc_address_ctx = gl;

        // Lets mpv pace frames against the display refresh instead of pushing
        // whatever it decoded as fast as it can.
        int advancedControl = 1;

        mpv_render_param params[] = {
            { MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL) },
            { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &initParams },
            { MPV_RENDER_PARAM_ADVANCED_CONTROL, &advancedControl },
            { MPV_RENDER_PARAM_INVALID, nullptr },
        };

        mpv_render_context *context = nullptr;
        const int status = mpv_render_context_create(&context, m_handle, params);
        if (status < 0) {
            qCWarning(lcMpvItem) << "mpv_render_context_create failed:" << mpv_error_string(status);
            reportError(QStringLiteral("libmpv 渲染初始化失败：%1").arg(QString::fromUtf8(mpv_error_string(status))));
            return;
        }

        m_initFailed = false;
        m_context = context;
        m_contextHandle = m_handle;
        m_gl = gl;
        if (m_bridge)
            mpv_render_context_set_update_callback(m_context, &onRenderUpdate, m_bridge.data());
        qCInfo(lcMpvItem) << "mpv OpenGL render context ready";
        notifyReady();
    }

    void destroyContext()
    {
        if (!m_context)
            return;

        mpv_render_context_set_update_callback(m_context, nullptr, nullptr);

        // Freeing touches the GL context that created it. If Qt already tore
        // the window down there is nothing safe left to do, so leak instead of
        // crashing on a dead context.
        if (QOpenGLContext::currentContext())
            mpv_render_context_free(m_context);
        else
            qCWarning(lcMpvItem) << "dropping the mpv render context without a GL context";

        m_context = nullptr;
        m_contextHandle = nullptr;
    }

    mpv_handle *m_handle = nullptr;
    mpv_handle *m_contextHandle = nullptr;
    mpv_render_context *m_context = nullptr;
    QSharedPointer<MpvUpdateBridge> m_bridge;
    /** Context the mpv render context was created on. */
    QOpenGLContext *m_gl = nullptr;
    QSize m_size;
    /** Set after a failed setup so the render loop does not retry every frame. */
    bool m_initFailed = false;
};

MpvItem::MpvItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
    , m_bridge(QSharedPointer<MpvUpdateBridge>::create())
{
    m_bridge->item = this;
    setFlag(ItemHasContents, true);
    // The texture is uploaded by mpv with the correct orientation, so Qt must
    // not mirror it a second time.
    setMirrorVertically(false);
}

MpvItem::~MpvItem()
{
    {
        QMutexLocker locker(&m_bridge->mutex);
        m_bridge->item = nullptr;
    }

    // The render context dies together with the renderer, so the engine has to
    // wait for a new one before it may load another file. Without this the next
    // visit to the player page would get "no render context set" and stay black.
    auto *mpvPlayer = qobject_cast<MpvPlayer *>(m_player.data());
    if (mpvPlayer && mpvPlayer->engine())
        mpvPlayer->engine()->notifyRenderContextLost();
}

void MpvItem::setPlayer(PlayerController *player)
{
    if (player == m_player)
        return;

    if (player && !qobject_cast<MpvPlayer *>(player))
        qCWarning(lcMpvItem) << "MpvItem expects the embedded libmpv backend, got mode" << player->mode();

    m_player = player;
    emit playerChanged();

    // A different engine can mean a different handle, so the renderer has to
    // throw its context away and set up a new one.
    update();

    // The context belongs to this item, not to the player: when it is already
    // live the new backend may load immediately.
    if (m_renderReady)
        handleRenderReady();
}

void MpvItem::handleRenderReady()
{
    m_renderReady = true;

    auto *mpvPlayer = qobject_cast<MpvPlayer *>(m_player.data());
    if (mpvPlayer && mpvPlayer->engine())
        mpvPlayer->engine()->notifyRenderReady();
}

QQuickFramebufferObject::Renderer *MpvItem::createRenderer() const
{
    return new MpvRenderer;
}

void MpvItem::setRenderError(const QString &message)
{
    if (message == m_renderError)
        return;
    m_renderError = message;
    emit renderErrorChanged();
}

mpv_handle *MpvItem::mpvHandle() const
{
    auto *mpvPlayer = qobject_cast<MpvPlayer *>(m_player.data());
    if (!mpvPlayer || !mpvPlayer->engine() || !mpvPlayer->engine()->isValid())
        return nullptr;
    return mpvPlayer->engine()->handle();
}

void MpvItem::scheduleUpdate()
{
    update();
}
