#pragma once

#include <QString>

class MpvEngine;
class PlayerController;
class QObject;

/** Creates the playback backend selected in the settings. */
namespace PlayerFactory {

/**
 * `mpvEngine` is the shared libmpv instance the embedded backend renders with;
 * it may be null when libmpv is not part of the build.
 */
PlayerController *create(const QString &mode,
                         const QString &ffplayPath,
                         MpvEngine *mpvEngine,
                         QObject *parent);

/**
 * True when this build can play video inside the application window. The
 * embedded backend is libmpv painting into the Qt Quick scene graph, so it is
 * available on every platform libmpv was found for.
 */
bool supportsEmbedded();

/** Pushes a new ffplay path into an existing backend; false if it uses none. */
bool applyTools(PlayerController *player, const QString &ffplayPath);

} // namespace PlayerFactory
