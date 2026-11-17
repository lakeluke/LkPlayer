#pragma once

#include "player/PlayerController.h"

class MpvEngine;
class QTimer;

/**
 * Embedded backend built on libmpv.
 *
 * Unlike the old ffplay window reparenting there is no child process and no
 * foreign window: mpv decodes in this process and `MpvItem` composites the
 * frames into the Qt Quick scene. That also means transport commands are real
 * API calls (absolute seek, volume in percent) instead of simulated key
 * presses.
 */
class MpvPlayer : public PlayerController
{
    Q_OBJECT

public:
    explicit MpvPlayer(MpvEngine *engine, QObject *parent = nullptr);
    ~MpvPlayer() override;

    /** False when the build has no libmpv; the factory then falls back. */
    static bool isAvailable();

    QString mode() const override { return QStringLiteral("embedded"); }
    /** libmpv decodes in-process, so ffmpeg/ffplay are not needed. */
    bool requiresExternalTools() const override { return false; }

    MpvEngine *engine() const { return m_engine; }

protected:
    void startPlayback(const QString &filePath, double startSeconds) override;
    void stopPlayback() override;
    void setPausedImpl(bool paused) override;
    void seekImpl(double seconds) override;
    void applyVolumeImpl() override;
    void applyMuteImpl() override;

private:
    /** Hands the queued file to mpv; runs once the render context is live. */
    void loadPending();

    MpvEngine *m_engine = nullptr;
    /** File requested before mpv could initialise its video output. */
    QString m_pendingFilePath;
    double m_pendingStartSeconds = 0.0;
    QTimer *m_loadTimeout = nullptr;
};
