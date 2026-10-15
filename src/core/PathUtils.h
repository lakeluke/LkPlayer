#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

/**
 * Small helpers around file names, sidecar resolution and tool discovery.
 * Everything here is pure and thread safe so the scanner can use it off the
 * GUI thread.
 */
namespace PathUtils {

const QStringList &videoExtensions();
const QStringList &subtitleExtensions();
const QStringList &imageExtensions();

bool isVideoFile(const QString &fileName);
bool isSubtitleFile(const QString &fileName);
bool isImageFile(const QString &fileName);

/** File name without its extension. */
QString baseName(const QString &fileName);

/** Folders that should never be scanned (metadata noise, recycle bins, ...). */
bool isIgnoredDirectory(const QString &folderName);

/**
 * A cheap snapshot of one directory. Building it once lets the scanner resolve
 * every sidecar file of every video without re-reading the directory.
 */
class DirIndex
{
public:
    void build(const QString &dirPath);

    bool isValid() const { return !m_path.isEmpty(); }
    QString path() const { return m_path; }
    const QStringList &files() const { return m_files; }
    const QStringList &subDirs() const { return m_subDirs; }

    /** Returns the real (correctly cased) file name, or an empty string. */
    QString resolve(const QString &fileName) const;
    bool hasFile(const QString &fileName) const;

private:
    QString m_path;
    QStringList m_files;
    QStringList m_subDirs;
    QHash<QString, QString> m_lookup;
};

QString findPoster(const DirIndex &index, const QString &videoBase);
QString findNfo(const DirIndex &index, const QString &videoBase, const QString &folderName);
QString findFanart(const DirIndex &index, const QString &videoBase, const QString &posterPath);
QStringList findSubtitles(const DirIndex &index, const QString &videoBase);
QStringList findStillImages(const DirIndex &index, const QString &videoBase, const QString &posterPath);

/** Folder level artwork (folder.jpg / poster.jpg / <folder name>.jpg). */
QString findFolderPoster(const DirIndex &index, const QString &folderName);

/** Same, but builds its own directory snapshot for one-off lookups. */
QString findFolderPoster(const QString &folderPath, const QString &folderName);

QString formatDuration(double seconds);
QString formatSize(qint64 bytes);

/** URL safe encoding used for the image provider identifiers. */
QString encodeId(const QString &text);
QString decodeId(const QString &text);

/** Fully qualified path of `<dir>/<baseName>[.exe]`, empty when missing. */
QString resolveToolIn(const QString &dir, const QString &toolBaseName);

/** Checks that both ffmpeg and ffplay live in `dir`. */
bool validateToolDir(const QString &dir, QString *error);

} // namespace PathUtils
