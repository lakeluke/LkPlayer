#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/FolderNode.h"
#include "core/MediaItem.h"

/**
 * What one directory looked like the last time it was read.
 *
 * `modifiedMs` is the directory's own modification time: on every supported
 * platform it changes when a child file is created, deleted or renamed, so an
 * unchanged timestamp means the directory does not have to be read again.
 * Editing a file's *content* does not touch it, which is why a full rescan can
 * be requested explicitly.
 */
struct LibraryFolderSnapshot
{
    FolderNode node;                  //!< folder node as shown by the UI (recursive counts)
    QHash<QString, int> directTagCounts; //!< tag counts of the videos stored directly here
    qint64 modifiedMs = 0;            //!< directory modification time
    QVector<MediaItem> items;         //!< videos stored directly in this folder
};

/**
 * On-disk index of a library scan, keyed by absolute folder path.
 *
 * The index lets a slow (mechanical) disk serve the whole library instantly on
 * start-up and then only re-read the directories whose timestamp changed.
 */
class LibraryIndex
{
public:
    void clear()
    {
        m_root.clear();
        m_folders.clear();
    }

    bool isEmpty() const { return m_folders.isEmpty(); }
    int folderCount() const { return m_folders.size(); }

    QString rootPath() const { return m_root; }
    void setRootPath(const QString &root) { m_root = root; }

    const QHash<QString, LibraryFolderSnapshot> &folders() const { return m_folders; }

    const LibraryFolderSnapshot *find(const QString &folderPath) const
    {
        const auto it = m_folders.constFind(folderPath);
        return it == m_folders.constEnd() ? nullptr : &it.value();
    }

    void insert(const LibraryFolderSnapshot &snapshot)
    {
        m_folders.insert(snapshot.node.path, snapshot);
    }

    void remove(const QString &folderPath) { m_folders.remove(folderPath); }

    /**
     * Reads the index stored in `filePath`. Returns false when the file is
     * missing, unreadable, written by another version or belongs to a different
     * `rootPath`.
     */
    static bool load(const QString &filePath, const QString &rootPath, LibraryIndex *out);

    /** Writes the index atomically (temp file + rename). */
    bool save(const QString &filePath) const;

private:
    QString m_root;
    QHash<QString, LibraryFolderSnapshot> m_folders;
};
