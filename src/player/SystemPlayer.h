#pragma once

#include "player/PlayerController.h"

/**
 * Hands the media file to whatever application Windows registered for its type
 * (`QDesktopServices::openUrl`, i.e. ShellExecute).
 *
 * Playback happens outside of this process, so position, duration, volume and
 * seeking are unavailable: the controller reports the `External` state and
 * `controllable` is false, which lets the UI dim the transport instead of
 * pretending to control something it cannot reach.
 */
class SystemPlayer : public PlayerController
{
    Q_OBJECT

public:
    explicit SystemPlayer(QObject *parent = nullptr);

    QString mode() const override { return QStringLiteral("system"); }
    bool isControllable() const override { return false; }

protected:
    void startPlayback(const QString &filePath, double startSeconds) override;
    void stopPlayback() override;

    // The external application owns playback, so these deliberately do nothing.
    void setPausedImpl(bool paused) override {}
    void seekImpl(double seconds) override {}
    void applyVolumeImpl() override {}
    void applyMuteImpl() override {}
};
