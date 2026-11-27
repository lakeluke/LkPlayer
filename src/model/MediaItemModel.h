#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QUrl>
#include <QVariantMap>

#include "core/MediaItem.h"

/**
 * Flat list of every video found in the library. Filtering happens in
 * MediaFilterProxyModel; this model only owns the data.
 */
class MediaItemModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        FilePathRole = Qt::UserRole + 1,
        FileNameRole,
        TitleRole,
        FolderPathRole,
        RelativeFolderRole,
        PosterPathRole,
        NfoPathRole,
        FanartPathRole,
        SubtitlesRole,
        StillImagesRole,
        TagsRole,
        GenresRole,
        ActorsRole,
        PlotRole,
        YearRole,
        PremieredRole,
        ModifiedRole,
        DurationRole,
        DurationTextRole,
        SizeTextRole,
        CoverUrlRole,
        HasPosterRole,
        HasSubtitlesRole,
        StillCountRole,
    };
    Q_ENUM(Role)

    explicit MediaItemModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void clear();
    void addItems(const MediaItemList &items);

    /**
     * Drops every row stored in `folderPath` and appends `items` instead. Used
     * by the incremental scan, which re-reads single folders after they changed.
     */
    void replaceFolder(const QString &folderPath, const MediaItemList &items);

    const MediaItem *itemAt(int row) const;
    /** Row of `filePath`, or -1. Lets the playlist scroll to the current video. */
    Q_INVOKABLE int indexOfPath(const QString &filePath) const;

    Q_INVOKABLE QVariantMap item(int row) const;

    /**
     * Reorders the flat list. `field` is one of "scan" (default traversal
     * order), "title", "name", "path" or "duration".
     */
    Q_INVOKABLE void sortBy(const QString &field, bool ascending);

signals:
    void countChanged();

private:
    QVariantMap toMap(const MediaItem &item) const;

    MediaItemList m_items;
    QHash<QString, int> m_pathIndex;
};
