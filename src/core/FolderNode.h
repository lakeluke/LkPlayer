#pragma once

#include <QHash>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * A directory inside the media library. Folder nodes drive the drill-down
 * navigation mode: every node knows its direct children so the UI can render
 * one level at a time.
 */
struct FolderNode
{
    QString path;         //!< absolute folder path
    QString name;         //!< folder name as shown in the UI
    QString parentPath;   //!< absolute path of the parent folder (empty for the root)
    QString posterPath;   //!< folder level artwork (poster.jpg / folder.jpg / <name>.jpg)
    int directVideoCount = 0;  //!< videos stored directly in this folder
    int totalVideoCount = 0;   //!< videos in this folder and all descendants
    int depth = 0;             //!< 0 for the library root
    QStringList childPaths;    //!< direct child folder paths, sorted by name
    QHash<QString, int> tagCounts; //!< tag -> number of videos carrying it (recursive)

    QStringList tags() const { return tagCounts.keys(); }
};

Q_DECLARE_METATYPE(FolderNode)

using FolderNodeList = QVector<FolderNode>;
Q_DECLARE_METATYPE(FolderNodeList)
