#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * One video discovered by the library scanner together with every sidecar
 * asset (poster, nfo, subtitles, stills) that could be associated with it.
 */
struct MediaItem
{
    QString filePath;        //!< absolute path of the video file
    QString fileName;        //!< file name including extension
    QString title;           //!< display title (nfo title, falls back to the file base name)
    QString folderPath;      //!< absolute path of the containing folder
    QString relativeFolder;  //!< folder path relative to the library root ("" for the root itself)
    QString posterPath;      //!< associated poster image, empty when none was found
    QString nfoPath;         //!< associated nfo file, empty when none was found
    QString fanartPath;      //!< associated fanart/backdrop image, empty when none was found
    QStringList subtitles;   //!< associated subtitle files
    QStringList stillImages; //!< associated stills / extrafanart
    QStringList tags;        //!< tags and genres harvested from the nfo, used for filtering
    QStringList genres;      //!< genres only (kept apart from tags for display)
    QStringList actors;      //!< actor names from the nfo
    QString plot;            //!< synopsis
    QString year;            //!< release year as text
    double duration = 0.0;   //!< duration in seconds, 0 when unknown (from nfo runtime)
    qint64 fileSize = 0;     //!< size in bytes
    qint64 lastModified = 0; //!< modification time (ms since epoch)
};

Q_DECLARE_METATYPE(MediaItem)

using MediaItemList = QVector<MediaItem>;
Q_DECLARE_METATYPE(MediaItemList)
