#pragma once

#include <QTimer>

#include "player/StandalonePlayer.h"

/**
 * Same ffplay plumbing as StandalonePlayer, but the ffplay window is moved
 * into a native child window that tracks the QML `VideoSurface` rectangle.
 *
 * SDL_WINDOWID is injected into the child process environment, and if the SDL
 * build ignores it we fall back to reparenting the ffplay window ourselves
 * once we located it by process id.
 */
class EmbeddedPlayer : public StandalonePlayer
{
    Q_OBJECT

public:
    explicit EmbeddedPlayer(QObject *parent = nullptr);
    ~EmbeddedPlayer() override;

    QString mode() const override { return QStringLiteral("embedded"); }

protected:
    void applyVideoRectImpl() override;
    void applyVideoVisibleImpl() override;
    void beforeStart() override;
    void afterStop() override;
    QStringList windowArguments() const override;
    QProcessEnvironment buildEnvironment() const override;
    void onWindowFound(quintptr handle) override;

private:
    void ensureHostWindow();
    void syncWindows();
    void attachVideoWindow(quintptr handle);

    /** True once the QML surface reported a rectangle large enough to show. */
    bool hasValidSurface() const;

    QTimer m_syncTimer;
    quintptr m_host = 0;
    quintptr m_videoWindow = 0;
};
