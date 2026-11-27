#pragma once

#include <QObject>
#include <QSize>
#include <QString>

struct mpv_handle;
struct mpv_event_property;

class QTimer;

/**
 * Owns a single libmpv client handle.
 *
 * Everything libmpv exposes as a "player" lives here: opening files, the
 * transport commands and the property observations that feed the UI. The
 * handle is created, driven and torn down on the thread that owns the object
 * (the GUI thread), which is why a short timer drains the event queue instead
 * of a blocking `mpv_wait_event()` - no locking is needed anywhere.
 *
 * Video output is *not* handled here. `vo=libmpv` only produces frames; they
 * are painted by `MpvItem` through libmpv's OpenGL render API on Qt's render
 * thread.
 */
class MpvEngine : public QObject
{
    Q_OBJECT

public:
    explicit MpvEngine(QObject *parent = nullptr);
    ~MpvEngine() override;

    /** False when libmpv could not be created or initialised. */
    bool isValid() const { return m_mpv != nullptr; }
    /** Raw handle; the render API in `MpvItem` needs it. */
    mpv_handle *handle() const { return m_mpv; }
    QString lastError() const { return m_error; }

    /**
     * True once MpvItem attached its OpenGL render context.
     *
     * mpv initialises its video output when a file is loaded, and `vo=libmpv`
     * refuses to start without a render context ("No render context set"),
     * which aborts playback. Files therefore may only be loaded after this
     * became true.
     */
    bool isRenderReady() const { return m_renderReady; }
    /** Called by the render item when its OpenGL context became usable. */
    void notifyRenderReady();
    /** Called when the item owning the render context goes away. */
    void notifyRenderContextLost();

    void load(const QString &filePath, double startSeconds);
    void stop();
    void setPaused(bool paused);
    void seekAbsolute(double seconds);
    void setVolume(int percent);
    void setMuted(bool muted);
    void setSpeed(double factor);

    /**
     * Tells mpv the size of the widget it is painted into so the video can be
     * zoomed to *cover* the area (no letterbox bars). Called by MpvItem when
     * its geometry changes.
     */
    void updateVideoFit(const QSize &areaSize);

signals:
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void pausedChanged(bool paused);
    void fileLoaded();
    void endOfFile();
    void errorOccurred(const QString &message);
    /** The render context is live; loading a file will now produce frames. */
    void renderReady();

private:
    void drainEvents();
    void handlePropertyChange(mpv_event_property *property);
    void reportError(const QString &message);
    void applyVideoFit();

    mpv_handle *m_mpv = nullptr;
    QTimer *m_pollTimer = nullptr;
    QString m_error;
    bool m_renderReady = false;
    /** Native video size reported by mpv (0 until a file is loaded). */
    QSize m_videoSize;
    /** Size of the widget mpv renders into. */
    QSize m_areaSize;
};
