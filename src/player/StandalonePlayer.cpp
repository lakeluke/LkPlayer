#include "player/StandalonePlayer.h"

#include <QLoggingCategory>

#include "player/FfplayProcess.h"

Q_LOGGING_CATEGORY(lcStandalone, "lk.player.standalone")

StandalonePlayer::StandalonePlayer(QObject *parent)
    : PlayerController(parent)
    , m_process(new FfplayProcess(this))
{
    m_process->setSeekInterval(10.0);

    connect(m_process, &FfplayProcess::positionChanged, this, [this](double seconds) {
        setPositionInternal(seconds);
    });
    connect(m_process, &FfplayProcess::durationChanged, this, [this](double seconds) {
        setDurationInternal(seconds);
    });
    connect(m_process, &FfplayProcess::windowFound, this, [this](quintptr handle) {
        onWindowFound(handle);
    });
    connect(m_process, &FfplayProcess::failed, this, [this](const QString &message) {
        qCWarning(lcStandalone) << "ffplay failed:" << message;
        setError(message);
        setState(Errored);
    });
    connect(m_process, &FfplayProcess::finished, this, [this](int exitCode) {
        Q_UNUSED(exitCode);
        afterStop();
        setPositionInternal(0.0);
        if (state() != Errored)
            setState(Stopped);
    });
}

StandalonePlayer::~StandalonePlayer() = default;

void StandalonePlayer::setProgram(const QString &program)
{
    if (m_process)
        m_process->setProgram(program);
}

QString StandalonePlayer::program() const
{
    return m_process ? m_process->program() : QString();
}

QProcessEnvironment StandalonePlayer::buildEnvironment() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Do not let ffplay cover the library window.
    if (!env.contains(QStringLiteral("SDL_VIDEO_CENTERED")))
        env.insert(QStringLiteral("SDL_VIDEO_CENTERED"), QStringLiteral("1"));
    return env;
}

void StandalonePlayer::startPlayback(const QString &filePath, double startSeconds)
{
    if (program().isEmpty()) {
        const QString message = tr("尚未配置 ffplay，请在设置中指定程序目录");
        setError(message);
        setState(Errored);
        return;
    }

    beforeStart();
    m_process->start(filePath, startSeconds, windowArguments(), title(), buildEnvironment());
}

void StandalonePlayer::restartAt(double seconds)
{
    if (filePath().isEmpty())
        return;
    qCInfo(lcStandalone) << "restarting playback at" << seconds << "seconds";
    setState(Loading);
    startPlayback(filePath(), seconds);
}

void StandalonePlayer::stopPlayback()
{
    if (m_process)
        m_process->stop();
    afterStop();
}

void StandalonePlayer::setPausedImpl(bool paused)
{
    Q_UNUSED(paused); // ffplay's 'p' toggles, and we only call this on changes
    if (m_process)
        m_process->sendShortcut('p');
}

void StandalonePlayer::seekImpl(double seconds)
{
    if (!m_process)
        return;

    if (m_process->requestRelativeSeek(seconds - position()))
        return;

    restartAt(seconds);
}

void StandalonePlayer::applyVolumeImpl()
{
    if (m_process)
        m_process->requestVolume(volume());
}

void StandalonePlayer::applyMuteImpl()
{
    if (m_process)
        m_process->requestMute(muted());
}
