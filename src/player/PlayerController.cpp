#include "player/PlayerController.h"

#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPlayer, "lk.player")

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
{
}

PlayerController::~PlayerController() = default;

QString PlayerController::stateName() const
{
    switch (m_state) {
    case Idle:
        return QStringLiteral("idle");
    case Loading:
        return QStringLiteral("loading");
    case Playing:
        return QStringLiteral("playing");
    case Paused:
        return QStringLiteral("paused");
    case Stopped:
        return QStringLiteral("stopped");
    case Errored:
        return QStringLiteral("error");
    case External:
        return QStringLiteral("external");
    }
    return QStringLiteral("idle");
}

double PlayerController::progress() const
{
    if (m_duration <= 0.0)
        return 0.0;
    return qBound(0.0, m_position / m_duration, 1.0);
}

void PlayerController::setWindowHandle(quintptr handle)
{
    if (handle == m_windowHandle)
        return;
    m_windowHandle = handle;
    applyVideoRectImpl();
}

void PlayerController::open(const QString &filePath, const QString &title)
{
    if (filePath.isEmpty())
        return;

    m_filePath = filePath;
    m_title = title.isEmpty() ? QFileInfo(filePath).completeBaseName() : title;
    m_position = 0.0;
    m_duration = 0.0;
    clearError();

    emit filePathChanged();
    emit positionChanged();
    emit durationChanged();

    setState(Loading);
    qCInfo(lcPlayer) << "opening" << filePath << "in" << mode() << "mode";
    startPlayback(filePath, 0.0);
}

void PlayerController::togglePlayPause()
{
    switch (m_state) {
    case Playing:
        setPaused(true);
        break;
    case Paused:
        setPaused(false);
        break;
    case Stopped:
    case Idle:
    case Errored:
        if (!m_filePath.isEmpty())
            open(m_filePath, m_title);
        break;
    default:
        break;
    }
}

void PlayerController::setPaused(bool paused)
{
    if (m_state != Playing && m_state != Paused)
        return;
    if (paused == isPaused())
        return;

    setPausedImpl(paused);
    setState(paused ? Paused : Playing);
}

void PlayerController::stop()
{
    stopPlayback();
    setPositionInternal(0.0);
    if (m_state != Idle)
        setState(Stopped);
}

bool PlayerController::seekTo(double seconds)
{
    if (m_state != Playing && m_state != Paused)
        return false;

    double target = seconds;
    if (m_duration > 0.0)
        target = qBound(0.0, target, m_duration);
    else
        target = qMax(0.0, target);

    if (qFuzzyCompare(target + 1.0, m_position + 1.0))
        return true;

    seekImpl(target);
    setPositionInternal(target);
    return true;
}

void PlayerController::seekRelative(double deltaSeconds)
{
    seekTo(m_position + deltaSeconds);
}

void PlayerController::setVolume(int volume)
{
    const int clamped = qBound(0, volume, 100);
    if (clamped == m_volume)
        return;
    m_volume = clamped;
    applyVolumeImpl();
    emit volumeChanged();
}

void PlayerController::setMuted(bool muted)
{
    if (muted == m_muted)
        return;
    m_muted = muted;
    applyMuteImpl();
    emit mutedChanged();
}

void PlayerController::toggleMute()
{
    setMuted(!m_muted);
}

void PlayerController::setVideoRect(const QRectF &devicePixelRect)
{
    if (devicePixelRect == m_videoRect)
        return;
    m_videoRect = devicePixelRect;
    applyVideoRectImpl();
}

void PlayerController::setVideoVisible(bool visible)
{
    if (visible == m_videoVisible)
        return;
    m_videoVisible = visible;
    applyVideoVisibleImpl();
}

void PlayerController::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged();
}

void PlayerController::setPositionInternal(double seconds)
{
    const double clamped = qMax(0.0, seconds);
    if (qFuzzyCompare(clamped + 1.0, m_position + 1.0))
        return;
    m_position = clamped;
    emit positionChanged();
}

void PlayerController::setDurationInternal(double seconds)
{
    const double clamped = qMax(0.0, seconds);
    if (qFuzzyCompare(clamped + 1.0, m_duration + 1.0))
        return;
    m_duration = clamped;
    emit durationChanged();
}

void PlayerController::setError(const QString &message)
{
    if (message == m_errorText)
        return;
    m_errorText = message;
    emit errorChanged();
}

void PlayerController::clearError()
{
    setError(QString());
}
