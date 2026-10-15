#include "player/EmbeddedPlayer.h"

#include <QLoggingCategory>

#include "player/FfplayProcess.h"

Q_LOGGING_CATEGORY(lcEmbedded, "lk.player.embedded")

namespace {
constexpr int kSyncIntervalMs = 250;

/** Below this the surface cannot be laid out yet, so the video stays hidden. */
constexpr qreal kMinSurfaceSize = 16.0;
} // namespace

EmbeddedPlayer::EmbeddedPlayer(QObject *parent)
    : StandalonePlayer(parent)
{
    m_syncTimer.setInterval(kSyncIntervalMs);
    connect(&m_syncTimer, &QTimer::timeout, this, &EmbeddedPlayer::syncWindows);
}

EmbeddedPlayer::~EmbeddedPlayer()
{
    afterStop();
}

bool EmbeddedPlayer::hasValidSurface() const
{
    const QRectF rect = videoRect();
    return rect.width() >= kMinSurfaceSize && rect.height() >= kMinSurfaceSize;
}

void EmbeddedPlayer::beforeStart()
{
    ensureHostWindow();
    m_syncTimer.start();
}

void EmbeddedPlayer::afterStop()
{
    m_syncTimer.stop();
    m_videoWindow = 0;

    if (m_host != 0) {
        NativeWindow::destroyWindow(m_host);
        m_host = 0;
    }
}

void EmbeddedPlayer::ensureHostWindow()
{
    if (windowHandle() == 0)
        return;

    const QRectF rect = videoRect();
    const bool valid = hasValidSurface();
    // The QML surface usually reports its rectangle only after `open()` has
    // already returned, so park the host at a placeholder size instead of
    // handing ffplay a 1x1 window it would have to grow out of.
    const int x = valid ? qRound(rect.x()) : 0;
    const int y = valid ? qRound(rect.y()) : 0;
    const int w = valid ? qMax(1, qRound(rect.width())) : static_cast<int>(kMinSurfaceSize);
    const int h = valid ? qMax(1, qRound(rect.height())) : static_cast<int>(kMinSurfaceSize);

    if (m_host == 0) {
        m_host = NativeWindow::createChildHost(windowHandle(), x, y, w, h);
        if (m_host == 0) {
            qCWarning(lcEmbedded) << "failed to create the native video host window";
            return;
        }
    } else {
        NativeWindow::setChildGeometry(m_host, x, y, w, h);
    }

    NativeWindow::setChildVisible(m_host, videoVisible() && valid);

    if (process())
        process()->setWindowHostHint(m_host);
}

QStringList EmbeddedPlayer::windowArguments() const
{
    // Only pin a size when the surface is already known; `syncWindows()` resizes
    // the window as soon as the real geometry arrives.
    if (!hasValidSurface())
        return QStringList { QStringLiteral("-noborder") };

    const QRectF rect = videoRect();
    QStringList args;
    args << QStringLiteral("-noborder") << QStringLiteral("-x")
         << QString::number(qMax(1, qRound(rect.width()))) << QStringLiteral("-y")
         << QString::number(qMax(1, qRound(rect.height())));
    return args;
}

QProcessEnvironment EmbeddedPlayer::buildEnvironment() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (m_host != 0)
        env.insert(QStringLiteral("SDL_WINDOWID"), QString::number(static_cast<qulonglong>(m_host)));
    return env;
}

void EmbeddedPlayer::applyVideoRectImpl()
{
    if (m_host == 0)
        return;
    syncWindows();
}

void EmbeddedPlayer::applyVideoVisibleImpl()
{
    const bool show = videoVisible() && hasValidSurface();
    if (m_host != 0)
        NativeWindow::setChildVisible(m_host, show);
    if (m_videoWindow != 0)
        NativeWindow::setChildVisible(m_videoWindow, show);
}

void EmbeddedPlayer::onWindowFound(quintptr handle)
{
    attachVideoWindow(handle);
}

void EmbeddedPlayer::attachVideoWindow(quintptr handle)
{
    if (handle == 0 || m_host == 0)
        return;

    if (m_videoWindow == handle)
        return;

    qCInfo(lcEmbedded) << "attaching ffplay window to the QML surface";
    NativeWindow::attachToHost(handle, m_host);
    m_videoWindow = handle;

    const QRectF rect = videoRect();
    NativeWindow::setChildGeometry(m_videoWindow, 0, 0, qMax(1, qRound(rect.width())), qMax(1, qRound(rect.height())));
    NativeWindow::setChildVisible(m_videoWindow, videoVisible() && hasValidSurface());
}

void EmbeddedPlayer::syncWindows()
{
    if (m_host == 0)
        return;

    const QRectF rect = videoRect();
    const int w = qMax(1, qRound(rect.width()));
    const int h = qMax(1, qRound(rect.height()));

    if (hasValidSurface()) {
        NativeWindow::setChildGeometry(m_host, qRound(rect.x()), qRound(rect.y()), w, h);
        // The layout just became usable: reveal the surface ffplay renders into.
        NativeWindow::setChildVisible(m_host, videoVisible());
        if (m_videoWindow != 0)
            NativeWindow::setChildVisible(m_videoWindow, videoVisible());
    }

    if (m_videoWindow == 0 && process() && process()->processId() != 0)
        attachVideoWindow(NativeWindow::findProcessWindow(process()->processId(), m_host));

    if (m_videoWindow != 0)
        NativeWindow::setChildGeometry(m_videoWindow, 0, 0, w, h);
}
