#include "player/MpvEngine.h"

#include <QLoggingCategory>
#include <QStringList>
#include <QTimer>

#include <mpv/client.h>

#include <cstring>

Q_LOGGING_CATEGORY(lcMpv, "lk.player.mpv")

namespace {

/**
 * How often the client handle is polled. mpv hands out events asynchronously;
 * 16ms keeps the progress bar smooth without measuring any audio or video
 * work, because the actual decoding runs on mpv's own threads.
 */
constexpr int kPollIntervalMs = 16;

QString errorText(int code)
{
    const char *text = mpv_error_string(code);
    return text ? QString::fromUtf8(text) : QStringLiteral("mpv error %1").arg(code);
}

void setOption(mpv_handle *mpv, const char *name, const char *value)
{
    if (mpv_set_option_string(mpv, name, value) < 0)
        qCWarning(lcMpv) << "could not set option" << name << "=" << value;
}

void observe(mpv_handle *mpv, const char *name, mpv_format format)
{
    if (mpv_observe_property(mpv, 0, name, format) < 0)
        qCWarning(lcMpv) << "could not observe property" << name;
}

void setDouble(mpv_handle *mpv, const char *name, double value)
{
    mpv_set_property(mpv, name, MPV_FORMAT_DOUBLE, &value);
}

void setFlag(mpv_handle *mpv, const char *name, bool value)
{
    int flag = value ? 1 : 0;
    mpv_set_property(mpv, name, MPV_FORMAT_FLAG, &flag);
}

} // namespace

MpvEngine::MpvEngine(QObject *parent)
    : QObject(parent)
{
    m_mpv = mpv_create();
    if (!m_mpv) {
        m_error = tr("无法创建 libmpv 实例");
        qCCritical(lcMpv) << m_error;
        return;
    }

    // Video goes through the render API, so mpv must not open a window of its
    // own and must not draw any overlay: the QML page draws the chrome.
    setOption(m_mpv, "vo", "libmpv");
    // The render context handed over by MpvItem is OpenGL. On Windows mpv would
    // otherwise initialise its video pipeline on d3d11, find no compatible
    // output and abort playback with "no audio or video data played".
    setOption(m_mpv, "gpu-api", "opengl");
    setOption(m_mpv, "osc", "no");
    setOption(m_mpv, "osd-level", "0");
    setOption(m_mpv, "input-default-bindings", "no");
    setOption(m_mpv, "input-vo-keyboard", "no");
    setOption(m_mpv, "terminal", "no");
    setOption(m_mpv, "audio-client-name", "LkPlayer");

    // Hardware decoding is the reason this backend exists at all: d3d11va on
    // Windows, vaapi/vdpau elsewhere, with a transparent software fallback.
    setOption(m_mpv, "hwdec", "auto-safe");
    setOption(m_mpv, "vd-lavc-dr", "yes");

    // Keep the handle alive between files instead of tearing mpv down on every
    // open, which is what makes switching items in the library instant.
    setOption(m_mpv, "keep-open", "no");
    setOption(m_mpv, "idle", "yes");

    // Match the ffplay backend's sidecar discovery: same-name subtitles and
    // audio tracks are picked up automatically.
    setOption(m_mpv, "sub-auto", "fuzzy");
    setOption(m_mpv, "audio-file-auto", "fuzzy");

    mpv_request_log_messages(m_mpv, "warn");

    const int status = mpv_initialize(m_mpv);
    if (status < 0) {
        m_error = tr("libmpv 初始化失败：%1").arg(errorText(status));
        qCCritical(lcMpv) << m_error;
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return;
    }

    observe(m_mpv, "time-pos", MPV_FORMAT_DOUBLE);
    observe(m_mpv, "duration", MPV_FORMAT_DOUBLE);
    observe(m_mpv, "pause", MPV_FORMAT_FLAG);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &MpvEngine::drainEvents);
    m_pollTimer->start();

    qCInfo(lcMpv) << "libmpv client initialised, version" << mpv_client_api_version();
}

MpvEngine::~MpvEngine()
{
    if (m_pollTimer)
        m_pollTimer->stop();

    if (!m_mpv)
        return;

    // The render context is freed by MpvItem before this point: it belongs to
    // the render thread and must not outlive the handle it renders.
    mpv_terminate_destroy(m_mpv);
    m_mpv = nullptr;
}

void MpvEngine::notifyRenderReady()
{
    if (m_renderReady)
        return;
    m_renderReady = true;
    qCInfo(lcMpv) << "render context attached; video output can now be initialised";
    emit renderReady();
}

void MpvEngine::notifyRenderContextLost()
{
    if (!m_renderReady)
        return;
    m_renderReady = false;
    qCInfo(lcMpv) << "render context detached; loading waits for a new one";
}

void MpvEngine::load(const QString &filePath, double startSeconds)
{
    if (!m_mpv)
        return;

    // mpv expects UTF-8 on every platform; QFile::encodeName() would use the
    // local 8-bit codepage and mangle non-ASCII paths on Windows.
    const QByteArray path = filePath.toUtf8();
    QByteArray startOption;

    // loadfile <url> <flags> <index> <options>: the start offset is an *option*
    // and therefore belongs in the fourth argument, not the third.
    const char *args[6] = { "loadfile", path.constData(), "replace", nullptr, nullptr, nullptr };
    if (startSeconds > 0.5) {
        startOption = "start=" + QByteArray::number(startSeconds, 'f', 3);
        args[3] = "-1"; // playlist index; -1 lets mpv pick the slot itself
        args[4] = startOption.constData();
    }

    const int status = mpv_command(m_mpv, args);
    if (status < 0) {
        reportError(tr("无法打开文件：%1").arg(errorText(status)));
        return;
    }
    qCInfo(lcMpv) << "loading" << filePath << "at" << startSeconds << "seconds";
}

void MpvEngine::stop()
{
    if (!m_mpv)
        return;
    mpv_command_string(m_mpv, "stop");
}

void MpvEngine::setPaused(bool paused)
{
    if (!m_mpv)
        return;
    setFlag(m_mpv, "pause", paused);
}

void MpvEngine::seekAbsolute(double seconds)
{
    if (!m_mpv)
        return;
    // An absolute seek instead of ffplay's "press the arrow key N times": no
    // 10 second quantisation and no process restart for long jumps.
    setDouble(m_mpv, "time-pos", seconds);
}

void MpvEngine::setVolume(int percent)
{
    if (!m_mpv)
        return;
    setDouble(m_mpv, "volume", qBound(0, percent, 100));
}

void MpvEngine::setMuted(bool muted)
{
    if (!m_mpv)
        return;
    setFlag(m_mpv, "mute", muted);
}

void MpvEngine::setSpeed(double factor)
{
    if (!m_mpv)
        return;
    setDouble(m_mpv, "speed", qBound(0.25, factor, 4.0));
}

void MpvEngine::drainEvents()
{
    if (!m_mpv)
        return;

    while (true) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (!event || event->event_id == MPV_EVENT_NONE)
            break;

        switch (event->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE:
            handlePropertyChange(static_cast<mpv_event_property *>(event->data));
            break;
        case MPV_EVENT_FILE_LOADED:
            emit fileLoaded();
            break;
        case MPV_EVENT_END_FILE: {
            auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (!end)
                break;
            if (end->reason == MPV_END_FILE_REASON_EOF) {
                emit endOfFile();
            } else if (end->reason == MPV_END_FILE_REASON_ERROR) {
                reportError(tr("播放失败：%1").arg(errorText(end->error)));
            }
            break;
        }
        case MPV_EVENT_LOG_MESSAGE:
            // Only "warn" and above are requested; keep them as breadcrumbs for
            // the log file instead of turning them into UI errors.
            if (auto *message = static_cast<mpv_event_log_message *>(event->data)) {
                if (message->text)
                    qCWarning(lcMpv) << QString::fromUtf8(message->prefix) << QString::fromUtf8(message->text).trimmed();
            }
            break;
        case MPV_EVENT_SHUTDOWN:
            qCInfo(lcMpv) << "mpv core shut down";
            break;
        default:
            break;
        }
    }
}

void MpvEngine::handlePropertyChange(mpv_event_property *property)
{
    if (!property || !property->name)
        return;

    // MPV_FORMAT_NONE means the property is currently unavailable (for example
    // time-pos while nothing is loaded); there is nothing to forward.
    if (property->format == MPV_FORMAT_NONE || !property->data)
        return;

    if (std::strcmp(property->name, "time-pos") == 0 && property->format == MPV_FORMAT_DOUBLE) {
        emit positionChanged(*static_cast<double *>(property->data));
    } else if (std::strcmp(property->name, "duration") == 0 && property->format == MPV_FORMAT_DOUBLE) {
        emit durationChanged(*static_cast<double *>(property->data));
    } else if (std::strcmp(property->name, "pause") == 0 && property->format == MPV_FORMAT_FLAG) {
        emit pausedChanged(*static_cast<int *>(property->data) != 0);
    }
}

void MpvEngine::reportError(const QString &message)
{
    m_error = message;
    qCWarning(lcMpv) << message;
    emit errorOccurred(message);
}
