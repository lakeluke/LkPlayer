#pragma once

#include <QString>

class PlayerController;
class QObject;

/** Creates the playback backend selected in the settings. */
namespace PlayerFactory {

PlayerController *create(const QString &mode, const QString &ffplayPath, QObject *parent);

/**
 * True when the platform can host a foreign window inside our own. Embedding
 * needs a windowing system we can manipulate directly (Win32); on Linux ffplay
 * is launched in its own window instead.
 */
bool supportsEmbedded();

/** Pushes a new ffplay path into an existing backend; false if it uses none. */
bool applyTools(PlayerController *player, const QString &ffplayPath);

} // namespace PlayerFactory
