#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

/**
 * Persists user preferences in an INI file inside the platform config
 * location. The ffmpeg/ffplay directory is what makes playback (and thumbnail
 * extraction) work, so it is validated eagerly.
 */
class SettingsStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")
    Q_PROPERTY(QString libraryRoot READ libraryRoot WRITE setLibraryRoot NOTIFY libraryRootChanged)
    Q_PROPERTY(QString ffmpegDir READ ffmpegDir WRITE setFfmpegDir NOTIFY ffmpegDirChanged)
    Q_PROPERTY(QString playMode READ playMode WRITE setPlayMode NOTIFY playModeChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(QString lastFolder READ lastFolder WRITE setLastFolder NOTIFY lastFolderChanged)
    Q_PROPERTY(bool toolsValid READ toolsValid NOTIFY toolsValidChanged)
    Q_PROPERTY(QString toolsStatus READ toolsStatus NOTIFY toolsValidChanged)
    Q_PROPERTY(QString configFilePath READ configFilePath CONSTANT)

public:
    explicit SettingsStore(QObject *parent = nullptr);

    void load();
    void sync();

    QString libraryRoot() const { return m_libraryRoot; }
    void setLibraryRoot(const QString &path);

    QString ffmpegDir() const { return m_ffmpegDir; }
    void setFfmpegDir(const QString &dir);

    /** "embedded", "standalone" or "system" */
    QString playMode() const { return m_playMode; }
    void setPlayMode(const QString &mode);

    /** "dark" or "light" */
    QString themeMode() const { return m_themeMode; }
    void setThemeMode(const QString &mode);

    /** "flat" or "drill" */
    QString viewMode() const { return m_viewMode; }
    void setViewMode(const QString &mode);

    QString lastFolder() const { return m_lastFolder; }
    void setLastFolder(const QString &folder);

    bool toolsValid() const { return m_toolsValid; }
    QString toolsStatus() const { return m_toolsStatus; }
    QString configFilePath() const { return m_configFilePath; }

    QString ffmpegPath() const;
    QString ffplayPath() const;

    /** Suggested tool directory: first ffplay/ffmpeg found on PATH. */
    static QString detectToolDir();

    Q_INVOKABLE bool validateTools(QString *errorMessage = nullptr) const;
    Q_INVOKABLE QString checkDirectory(const QString &dir) const;

signals:
    void libraryRootChanged();
    void ffmpegDirChanged();
    void playModeChanged();
    void themeModeChanged();
    void viewModeChanged();
    void lastFolderChanged();
    void toolsValidChanged();

private:
    void revalidate();

    QString m_libraryRoot;
    QString m_ffmpegDir;
    QString m_playMode = QStringLiteral("standalone");
    QString m_themeMode = QStringLiteral("dark");
    QString m_viewMode = QStringLiteral("flat");
    QString m_lastFolder;
    bool m_toolsValid = false;
    QString m_toolsStatus;
    QString m_configFilePath;
};
