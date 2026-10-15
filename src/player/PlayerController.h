#pragma once

#include <QObject>
#include <QRectF>
#include <QString>
#include <QtQml/qqmlregistration.h>

/**
 * Backend agnostic playback contract.
 *
 * The public API implements the shared bookkeeping (state machine, volume,
 * mute, position) while the actual process handling lives behind a handful of
 * protected hooks. Two implementations exist: an ffplay process in its own
 * window and an ffplay process reparented into the Qt Quick window.
 */
class PlayerController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by AppController")

    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(QString title READ title NOTIFY filePathChanged)
    Q_PROPERTY(QString stateName READ stateName NOTIFY stateChanged)
    Q_PROPERTY(int state READ stateCode NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY stateChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ isPaused NOTIFY stateChanged)
    Q_PROPERTY(bool embedded READ isEmbedded NOTIFY playerChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY playerChanged)
    /** False for backends that hand the file to another application. */
    Q_PROPERTY(bool controllable READ isControllable NOTIFY playerChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double progress READ progress NOTIFY positionChanged)
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
    Q_PROPERTY(bool hasError READ hasError NOTIFY errorChanged)

public:
    enum State {
        Idle,
        Loading,
        Playing,
        Paused,
        Stopped,
        Errored,
        /** Playback was handed to another application; we cannot control it. */
        External,
    };
    Q_ENUM(State)

    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;

    QString filePath() const { return m_filePath; }
    QString title() const { return m_title; }
    State state() const { return m_state; }
    int stateCode() const { return static_cast<int>(m_state); }
    QString stateName() const;
    bool isPlaying() const { return m_state == Playing; }
    bool isPaused() const { return m_state == Paused; }
    bool isActive() const
    {
        return m_state == Loading || m_state == Playing || m_state == Paused || m_state == External;
    }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    double progress() const;
    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    QString errorText() const { return m_errorText; }
    bool hasError() const { return !m_errorText.isEmpty(); }

    /** "embedded", "standalone" or "system". */
    virtual QString mode() const = 0;
    bool isEmbedded() const { return mode() == QLatin1String("embedded"); }

    /** False when another application owns playback (seek/volume do not apply). */
    virtual bool isControllable() const { return true; }

    void setWindowHandle(quintptr handle);

public slots:
    void open(const QString &filePath, const QString &title = QString());
    void togglePlayPause();
    void setPaused(bool paused);
    void stop();
    bool seekTo(double seconds);
    void seekRelative(double deltaSeconds);
    void setVolume(int volume);
    void setMuted(bool muted);
    void toggleMute();
    void setVideoRect(const QRectF &devicePixelRect);
    void setVideoVisible(bool visible);

signals:
    void playerChanged();
    void filePathChanged();
    void stateChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void mutedChanged();
    void errorChanged();

protected:
    // --- backend hooks -----------------------------------------------------
    virtual void startPlayback(const QString &filePath, double startSeconds) = 0;
    virtual void stopPlayback() = 0;
    virtual void setPausedImpl(bool paused) = 0;
    virtual void seekImpl(double seconds) = 0;
    virtual void applyVolumeImpl() = 0;
    virtual void applyMuteImpl() = 0;
    virtual void applyVideoRectImpl() {}
    virtual void applyVideoVisibleImpl() {}

    void setState(State state);
    void setPositionInternal(double seconds);
    void setDurationInternal(double seconds);
    void setError(const QString &message);
    void clearError();

    quintptr windowHandle() const { return m_windowHandle; }
    QRectF videoRect() const { return m_videoRect; }
    bool videoVisible() const { return m_videoVisible; }

private:
    QString m_filePath;
    QString m_title;
    State m_state = Idle;
    double m_position = 0.0;
    double m_duration = 0.0;
    int m_volume = 100;
    bool m_muted = false;
    QString m_errorText;
    quintptr m_windowHandle = 0;
    QRectF m_videoRect;
    bool m_videoVisible = false;
};
