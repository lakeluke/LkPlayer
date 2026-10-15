#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QtQml/qqmlregistration.h>

#include "core/FolderNode.h"

/**
 * Direct child folders of the folder the user is currently browsing.
 * Used by the drill-down navigation grid and by the sidebar tree.
 */
class FolderListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString currentFolder READ currentFolder WRITE setCurrentFolder NOTIFY currentFolderChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        CoverUrlRole,
        HasCoverRole,
        VideoCountRole,
        RelativePathRole,
        DepthRole,
    };
    Q_ENUM(Role)

    explicit FolderListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** Replaces the folder tree and rebuilds the visible child list. */
    void setFolders(const FolderNodeList &nodes);

    /** First video found below a folder, used as fallback cover artwork. */
    void setCoverHints(const QHash<QString, QString> &folderToVideo);

    void setRootPath(const QString &rootPath);

    QString currentFolder() const { return m_currentFolder; }
    void setCurrentFolder(const QString &folder);

    Q_INVOKABLE QString pathAt(int row) const;

signals:
    void countChanged();
    void currentFolderChanged();

private:
    void rebuild();

    QHash<QString, FolderNode> m_nodes;
    QHash<QString, QString> m_coverHints;
    QString m_currentFolder;
    QString m_rootPath;
    QStringList m_children;
};
