#pragma once

#include <QString>

class PlayerController;
class QObject;

/** Creates the playback backend selected in the settings. */
namespace PlayerFactory {

PlayerController *create(const QString &mode, const QString &ffplayPath, QObject *parent);

/** Pushes a new ffplay path into an existing backend; false if it uses none. */
bool applyTools(PlayerController *player, const QString &ffplayPath);

} // namespace PlayerFactory
