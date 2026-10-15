#pragma once

#include <QProcessEnvironment>

#include "player/PlayerController.h"

class FfplayProcess;

/**
 * Plays media through an ffplay process that owns its own top level window.
 * This is the most robust backend and the default one.
 */
class StandalonePlayer : public PlayerController
{
    Q_OBJECT

public:
    explicit StandalonePlayer(QObject *parent = nullptr);
    ~StandalonePlayer() override;

    void setProgram(const QString &program);
    QString program() const;

    QString mode() const override { return QStringLiteral("standalone"); }

protected:
    // PlayerController hooks ------------------------------------------------
    void startPlayback(const QString &filePath, double startSeconds) override;
    void stopPlayback() override;
    void setPausedImpl(bool paused) override;
    void seekImpl(double seconds) override;
    void applyVolumeImpl() override;
    void applyMuteImpl() override;

    // Window behaviour, overridden by the embedded backend ------------------
    virtual void beforeStart() {}
    virtual void afterStop() {}
    virtual QStringList windowArguments() const { return QStringList(); }
    virtual QProcessEnvironment buildEnvironment() const;
    virtual void onWindowFound(quintptr handle) { Q_UNUSED(handle); }

    FfplayProcess *process() const { return m_process; }

    /** Re-launches ffplay at `seconds` (used for large seeks). */
    void restartAt(double seconds);

private:
    FfplayProcess *m_process = nullptr;
};
