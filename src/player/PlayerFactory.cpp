#include "player/PlayerFactory.h"

#include <QLoggingCategory>

#include "player/EmbeddedPlayer.h"
#include "player/PlayerController.h"
#include "player/StandalonePlayer.h"
#include "player/SystemPlayer.h"

Q_LOGGING_CATEGORY(lcPlayerFactory, "lk.player.factory")

namespace PlayerFactory {

PlayerController *create(const QString &mode, const QString &ffplayPath, QObject *parent)
{
    PlayerController *player = nullptr;
    if (mode == QLatin1String("embedded"))
        player = new EmbeddedPlayer(parent);
    else if (mode == QLatin1String("system"))
        player = new SystemPlayer(parent);
    else
        player = new StandalonePlayer(parent);

    // The system backend never spawns ffplay, so it ignores the tool path.
    if (applyTools(player, ffplayPath))
        qCInfo(lcPlayerFactory) << "created" << player->mode() << "player with" << ffplayPath;
    else
        qCInfo(lcPlayerFactory) << "created" << player->mode() << "player (no external tools)";
    return player;
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
