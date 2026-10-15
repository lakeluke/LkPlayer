#include "settings/SettingsStore.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSettings>
#include <QStandardPaths>

#include "app/AppPaths.h"
#include "core/PathUtils.h"

Q_LOGGING_CATEGORY(lcSettings, "lk.settings")

namespace {
const QString kLibraryRoot = QStringLiteral("library/root");
const QString kFfmpegDir = QStringLiteral("tools/directory");
const QString kPlayMode = QStringLiteral("player/mode");
const QString kThemeMode = QStringLiteral("ui/theme");
const QString kViewMode = QStringLiteral("ui/viewMode");
const QString kLastFolder = QStringLiteral("ui/lastFolder");

bool isPlayMode(const QString &mode)
{
    return mode == QLatin1String("embedded") || mode == QLatin1String("standalone")
           || mode == QLatin1String("system");
}
} // namespace

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent)
{
    m_configFilePath = AppPaths::settingsFile();
}

void SettingsStore::load()
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    m_libraryRoot = settings.value(kLibraryRoot).toString();
    m_ffmpegDir = settings.value(kFfmpegDir).toString();
    if (m_ffmpegDir.isEmpty())
        m_ffmpegDir = detectToolDir();

    m_playMode = settings.value(kPlayMode, QStringLiteral("standalone")).toString();
    if (!isPlayMode(m_playMode))
        m_playMode = QStringLiteral("standalone");

    m_themeMode = settings.value(kThemeMode, QStringLiteral("dark")).toString();
    if (m_themeMode != QLatin1String("dark") && m_themeMode != QLatin1String("light"))
        m_themeMode = QStringLiteral("dark");

    m_viewMode = settings.value(kViewMode, QStringLiteral("flat")).toString();
    if (m_viewMode != QLatin1String("flat") && m_viewMode != QLatin1String("drill"))
        m_viewMode = QStringLiteral("flat");

    m_lastFolder = settings.value(kLastFolder).toString();

    qCInfo(lcSettings) << "settings loaded from" << m_configFilePath;

    emit libraryRootChanged();
    emit ffmpegDirChanged();
    emit playModeChanged();
    emit themeModeChanged();
    emit viewModeChanged();
    emit lastFolderChanged();
    revalidate();
}

void SettingsStore::sync()
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    settings.setValue(kLibraryRoot, m_libraryRoot);
    settings.setValue(kFfmpegDir, m_ffmpegDir);
    settings.setValue(kPlayMode, m_playMode);
    settings.setValue(kThemeMode, m_themeMode);
    settings.setValue(kViewMode, m_viewMode);
    settings.setValue(kLastFolder, m_lastFolder);
    settings.sync();

    if (settings.status() != QSettings::NoError)
        qCWarning(lcSettings) << "failed to write settings:" << m_configFilePath;
}

void SettingsStore::setLibraryRoot(const QString &path)
{
    const QString cleaned = path.isEmpty() ? QString() : QDir::cleanPath(path);
    if (cleaned == m_libraryRoot)
        return;
    m_libraryRoot = cleaned;
    emit libraryRootChanged();
    sync();
}

void SettingsStore::setFfmpegDir(const QString &dir)
{
    const QString cleaned = dir.isEmpty() ? QString() : QDir::cleanPath(dir);
    if (cleaned == m_ffmpegDir)
        return;
    m_ffmpegDir = cleaned;
    emit ffmpegDirChanged();
    revalidate();
    sync();
}

void SettingsStore::setPlayMode(const QString &mode)
{
    const QString normalised = isPlayMode(mode) ? mode : QStringLiteral("standalone");
    if (normalised == m_playMode)
        return;
    m_playMode = normalised;
    emit playModeChanged();
    sync();
}

void SettingsStore::setThemeMode(const QString &mode)
{
    const QString normalised = (mode == QLatin1String("light")) ? QStringLiteral("light") : QStringLiteral("dark");
    if (normalised == m_themeMode)
        return;
    m_themeMode = normalised;
    emit themeModeChanged();
    sync();
}

void SettingsStore::setViewMode(const QString &mode)
{
    const QString normalised = (mode == QLatin1String("drill")) ? QStringLiteral("drill") : QStringLiteral("flat");
    if (normalised == m_viewMode)
        return;
    m_viewMode = normalised;
    emit viewModeChanged();
    sync();
}

void SettingsStore::setLastFolder(const QString &folder)
{
    const QString cleaned = folder.isEmpty() ? QString() : QDir::cleanPath(folder);
    if (cleaned == m_lastFolder)
        return;
    m_lastFolder = cleaned;
    emit lastFolderChanged();
    sync();
}

QString SettingsStore::ffmpegPath() const
{
    return PathUtils::resolveToolIn(m_ffmpegDir, QStringLiteral("ffmpeg"));
}

QString SettingsStore::ffplayPath() const
{
    return PathUtils::resolveToolIn(m_ffmpegDir, QStringLiteral("ffplay"));
}

QString SettingsStore::detectToolDir()
{
    const QString ffplay = QStandardPaths::findExecutable(QStringLiteral("ffplay"));
    if (!ffplay.isEmpty())
        return QFileInfo(ffplay).absolutePath();

    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (!ffmpeg.isEmpty())
        return QFileInfo(ffmpeg).absolutePath();

    return QString();
}

bool SettingsStore::validateTools(QString *errorMessage) const
{
    return PathUtils::validateToolDir(m_ffmpegDir, errorMessage);
}

QString SettingsStore::checkDirectory(const QString &dir) const
{
    QString error;
    if (PathUtils::validateToolDir(dir, &error))
        return QString();
    return error;
}

void SettingsStore::revalidate()
{
    QString error;
    const bool valid = PathUtils::validateToolDir(m_ffmpegDir, &error);
    const QString status = valid ? QStringLiteral("ffmpeg 与 ffplay 均可用") : error;
    if (valid == m_toolsValid && status == m_toolsStatus)
        return;
    m_toolsValid = valid;
    m_toolsStatus = status;
    emit toolsValidChanged();
}
