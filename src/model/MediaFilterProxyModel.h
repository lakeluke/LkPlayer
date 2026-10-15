#pragma once

#include <QSortFilterProxyModel>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class MediaItemModel;

/**
 * Applies the browsing scope (whole library vs. one folder), the tag selection
 * and the free text search on top of MediaItemModel.
 */
class MediaFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(QString currentFolder READ currentFolder WRITE setCurrentFolder NOTIFY currentFolderChanged)
    Q_PROPERTY(QStringList selectedTags READ selectedTags WRITE setSelectedTags NOTIFY selectedTagsChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QStringList availableTags READ availableTags NOTIFY availableTagsChanged)
    Q_PROPERTY(bool hasTagFilter READ hasTagFilter NOTIFY selectedTagsChanged)

public:
    explicit MediaFilterProxyModel(QObject *parent = nullptr);

    int count() const { return rowCount(); }

    QString viewMode() const { return m_viewMode; }
    void setViewMode(const QString &mode);

    QString currentFolder() const { return m_currentFolder; }
    void setCurrentFolder(const QString &folder);

    QStringList selectedTags() const { return m_selectedTags; }
    void setSelectedTags(const QStringList &tags);
    Q_INVOKABLE void toggleTag(const QString &tag);
    Q_INVOKABLE void clearTags();

    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &text);

    QStringList availableTags() const;
    bool hasTagFilter() const { return !m_selectedTags.isEmpty(); }

    Q_INVOKABLE QString filePathAt(int row) const;

signals:
    void countChanged();
    void viewModeChanged();
    void currentFolderChanged();
    void selectedTagsChanged();
    void searchTextChanged();
    void availableTagsChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    void refreshFilter();
    bool rowInScope(int sourceRow) const;
    MediaItemModel *sourceItemModel() const;

    QString m_viewMode = QStringLiteral("flat");
    QString m_currentFolder;
    QStringList m_selectedTags;
    QString m_searchText;

    mutable QStringList m_availableTags;
    mutable bool m_availableTagsDirty = true;
};
