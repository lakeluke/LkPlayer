#include "player/SystemPlayer.h"

#include <QDesktopServices>
#include <QLoggingCategory>
#include <QUrl>

Q_LOGGING_CATEGORY(lcSystem, "lk.player.system")

SystemPlayer::SystemPlayer(QObject *parent)
    : PlayerController(parent)
{
}

void SystemPlayer::startPlayback(const QString &filePath, double startSeconds)
{
    Q_UNUSED(startSeconds); // The registered application decides where to start.

    const QUrl url = QUrl::fromLocalFile(filePath);
    qCInfo(lcSystem) << "handing" << filePath << "to the registered application";

    if (!QDesktopServices::openUrl(url)) {
        const QString message = tr("未能用系统默认播放器打开该视频");
        qCWarning(lcSystem) << message;
        setError(message);
        setState(Errored);
        return;
    }

    setState(External);
}

void SystemPlayer::stopPlayback()
{
    // The external window belongs to another process; there is nothing to stop.
    setError(QString());
}
