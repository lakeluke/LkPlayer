#include "player/MpvPlayer.h"

#include <QLoggingCategory>
#include <QTimer>

#include "player/MpvEngine.h"

Q_LOGGING_CATEGORY(lcMpvPlayer, "lk.player.mpv.player")

namespace {
/**
 * Safety net for the "wait for the render context" gate below. Reaching it
 * means the item never rendered, so playback is attempted anyway: a visible
 * error beats a permanently silent player.
 */
constexpr int kRenderWaitTimeoutMs = 3000;
} // namespace

MpvPlayer::MpvPlayer(MpvEngine *engine, QObject *parent)
    : PlayerController(parent)
    , m_engine(engine)
{
    Q_ASSERT(m_engine);

    connect(m_engine, &MpvEngine::positionChanged, this, [this](double seconds) {
        setPositionInternal(seconds);
    });
    connect(m_engine, &MpvEngine::durationChanged, this, [this](double seconds) {
        setDurationInternal(seconds);
    });
    connect(m_engine, &MpvEngine::pausedChanged, this, [this](bool paused) {
        // mpv echoes every pause change, including the ones we requested. Only
        // the states the UI knows about may be flipped here.
        if (state() != Playing && state() != Paused)
            return;
        setState(paused ? Paused : Playing);
    });
    connect(m_engine, &MpvEngine::fileLoaded, this, [this]() {
        if (state() == Loading || state() == Playing || state() == Paused)
            setState(isPaused() ? Paused : Playing);
    });
    connect(m_engine, &MpvEngine::endOfFile, this, [this]() {
        qCInfo(lcMpvPlayer) << "playback reached the end of the file";
        setPositionInternal(0.0);
        setState(Stopped);
    });
    connect(m_engine, &MpvEngine::errorOccurred, this, [this](const QString &message) {
        setError(message);
        setState(Errored);
    });
    connect(m_engine, &MpvEngine::renderReady, this, &MpvPlayer::loadPending);

    m_loadTimeout = new QTimer(this);
    m_loadTimeout->setSingleShot(true);
    connect(m_loadTimeout, &QTimer::timeout, this, [this]() {
        qCWarning(lcMpvPlayer) << "render context did not appear in time, loading anyway";
        loadPending();
    });
}

MpvPlayer::~MpvPlayer() = default;

bool MpvPlayer::isAvailable()
{
#ifdef LKPLAYER_HAVE_MPV
    return true;
#else
    return false;
#endif
}

void MpvPlayer::startPlayback(const QString &filePath, double startSeconds)
{
    if (!m_engine || !m_engine->isValid()) {
        setError(tr("libmpv 不可用：%1")
                     .arg(m_engine ? m_engine->lastError() : tr("未初始化")));
        setState(Errored);
        return;
    }

    // The engine is shared across player instances, so the mixer state the UI
    // is showing has to be pushed again before a new file starts.
    m_engine->setVolume(volume());
    m_engine->setMuted(muted());
    m_engine->setPaused(false);

    m_pendingFilePath = filePath;
    m_pendingStartSeconds = startSeconds;

    if (m_engine->isRenderReady()) {
        loadPending();
        return;
    }

    // `vo=libmpv` cannot initialise before MpvItem attached its render context;
    // mpv would abort with "no audio or video data played" and no frames at
    // all. The context needs a scene graph frame, so the load has to wait.
    qCInfo(lcMpvPlayer) << "waiting for the render context before loading" << filePath;
    m_loadTimeout->start(kRenderWaitTimeoutMs);
}

void MpvPlayer::loadPending()
{
    if (m_loadTimeout)
        m_loadTimeout->stop();
    if (m_pendingFilePath.isEmpty() || !m_engine)
        return;

    const QString path = m_pendingFilePath;
    const double start = m_pendingStartSeconds;
    m_pendingFilePath.clear();
    m_engine->load(path, start);
}

void MpvPlayer::stopPlayback()
{
    m_pendingFilePath.clear();
    if (m_loadTimeout)
        m_loadTimeout->stop();
    if (m_engine)
        m_engine->stop();
}

void MpvPlayer::setPausedImpl(bool paused)
{
    if (m_engine)
        m_engine->setPaused(paused);
}

void MpvPlayer::seekImpl(double seconds)
{
    if (m_engine)
        m_engine->seekAbsolute(seconds);
}

void MpvPlayer::applyVolumeImpl()
{
    if (m_engine)
        m_engine->setVolume(volume());
}

void MpvPlayer::applyMuteImpl()
{
    if (m_engine)
        m_engine->setMuted(muted());
}
