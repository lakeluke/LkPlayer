#include "player/PlayerFactory.h"

#include <QLoggingCategory>

#include "player/MpvEngine.h"
#include "player/MpvPlayer.h"
#include "player/PlayerController.h"
#include "player/StandalonePlayer.h"
#include "player/SystemPlayer.h"

Q_LOGGING_CATEGORY(lcPlayerFactory, "lk.player.factory")

namespace PlayerFactory {

PlayerController *create(const QString &mode,
                         const QString &ffplayPath,
                         MpvEngine *mpvEngine,
                         QObject *parent)
{
    PlayerController *player = nullptr;

    // The embedded backend needs a usable libmpv instance; without one the
    // request degrades to ffplay in its own window instead of failing.
    if (mode == QLatin1String("embedded") && supportsEmbedded() && mpvEngine && mpvEngine->isValid())
        player = new MpvPlayer(mpvEngine, parent);
    else if (mode == QLatin1String("system"))
        player = new SystemPlayer(parent);
    else
        player = new StandalonePlayer(parent);

    if (mode == QLatin1String("embedded") && player->mode() != QLatin1String("embedded"))
        qCWarning(lcPlayerFactory) << "embedded mode unavailable, falling back to" << player->mode();

    // The system and libmpv backends never spawn ffplay, so they ignore the path.
    if (applyTools(player, ffplayPath))
        qCInfo(lcPlayerFactory) << "created" << player->mode() << "player with" << ffplayPath;
    else
        qCInfo(lcPlayerFactory) << "created" << player->mode() << "player (no external tools)";
    return player;
}

bool supportsEmbedded()
{
    return MpvPlayer::isAvailable();
}

bool applyTools(PlayerController *player, const QString &ffplayPath)
{
    auto *standalone = qobject_cast<StandalonePlayer *>(player);
    if (!standalone)
        return false;
    standalone->setProgram(ffplayPath);
    return true;
}

} // namespace PlayerFactory
