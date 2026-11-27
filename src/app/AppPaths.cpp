#include "app/AppPaths.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace AppPaths {
namespace {

/// Drop `portable.txt` next to the executable to keep the settings there.
const QString kPortableMarker = QStringLiteral("portable.txt");
const QString kPortableDataDir = QStringLiteral("Data");
const QString kSettingsName = QStringLiteral("lkplayer.ini");

QString applicationDir()
{
    return QCoreApplication::applicationDirPath();
}

bool detectPortable()
{
    return QFileInfo::exists(QDir(applicationDir()).absoluteFilePath(kPortableMarker));
}

} // namespace

bool isPortable()
{
    static const bool portable = detectPortable();
    return portable;
}

QString dataRoot()
{
    if (isPortable())
        return QDir(applicationDir()).absoluteFilePath(kPortableDataDir);

    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

QString settingsFile()
{
    QString dir;
    if (isPortable()) {
        dir = dataRoot();
    } else {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    }
    QDir().mkpath(dir);
    return QDir(dir).absoluteFilePath(kSettingsName);
}

QString logDir()
{
    const QString dir = qEnvironmentVariable("LKPLAYER_LOG_DIR", dataRoot());
    QDir().mkpath(dir);
    return dir;
}

QString coverDatabaseFile()
{
    // Covers are durable state, not OS-managed cache, so they live in the
    // application data directory and travel with portable installs.
    const QString dir = dataRoot();
    QDir().mkpath(dir);
    return QDir(dir).absoluteFilePath(QStringLiteral("covers.db"));
}

QString scanIndexFile(const QString &rootPath)
{
    const QString base = isPortable() ? dataRoot()
                                      : QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(base);

    const QString cleaned = QDir::cleanPath(rootPath);
    const QByteArray digest =
        QCryptographicHash::hash(cleaned.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QDir(base).absoluteFilePath(QStringLiteral("scan-") + QString::fromLatin1(digest)
                                       + QStringLiteral(".bin"));
}

} // namespace AppPaths
