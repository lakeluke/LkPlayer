#include "thumb/ThumbnailCache.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

#include "app/AppPaths.h"

Q_LOGGING_CATEGORY(lcThumbCache, "lk.thumb.cache")

ThumbnailCache::ThumbnailCache(QObject *parent)
    : QObject(parent)
{
    m_dbPath = AppPaths::coverDatabaseFile();

    // The previous implementation stored one JPG per cover in a directory.
    // Drop it once so a stale folder doesn't linger after the move to SQLite.
    const QString legacyDir = QDir(AppPaths::dataRoot()).absoluteFilePath(QStringLiteral("covers"));
    if (QDir(legacyDir).exists())
        QDir(legacyDir).removeRecursively();
}

QString ThumbnailCache::keyFor(const QString &videoPath, qint64 fileSize, qint64 lastModified) const
{
    const QString absPath = QFileInfo(videoPath).absoluteFilePath();

    qint64 size = fileSize;
    qint64 mtime = lastModified;
    if (size < 0 || mtime < 0) {
        const QFileInfo info(absPath);
        size = info.size();
        mtime = info.lastModified().toMSecsSinceEpoch();
    }

    const QString identity = QStringLiteral("%1|%2|%3").arg(absPath).arg(size).arg(mtime);
    const QByteArray digest = QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha1);
    return QString::fromLatin1(digest.toHex());
}

QString ThumbnailCache::tempFileFor(const QString &videoPath, qint64 fileSize, qint64 lastModified) const
{
    return QDir::temp().absoluteFilePath(QStringLiteral("lkcover-%1.jpg").arg(keyFor(videoPath, fileSize, lastModified)));
}

QSqlDatabase ThumbnailCache::connection() const
{
    // One connection per calling thread: QSqlDatabase connections are bound to
    // the thread that created them, and `lookup()` runs on the Qt Quick image
    // loading threads while `store()` runs on the GUI thread.
    const QString name = QStringLiteral("lkcover_%1").arg(quintptr(QThread::currentThreadId()));
    if (QSqlDatabase::contains(name))
        return QSqlDatabase::database(name);

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(m_dbPath);
    if (db.open()) {
        ensureSchema(db);
    } else {
        qCWarning(lcThumbCache) << "failed to open cover database" << m_dbPath << db.lastError().text();
    }
    return db;
}

void ThumbnailCache::ensureSchema(QSqlDatabase &db) const
{
    QSqlQuery query(db);
    // busy_timeout first so a concurrent first-time setup waits instead of failing.
    query.exec(QStringLiteral("PRAGMA busy_timeout=5000"));
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    query.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS covers ("
                              "key TEXT PRIMARY KEY, "
                              "data BLOB NOT NULL, "
                              "last_used INTEGER NOT NULL)"));
}

bool ThumbnailCache::contains(const QString &videoPath, qint64 fileSize, qint64 lastModified) const
{
    if (videoPath.isEmpty())
        return false;

    QSqlDatabase db = connection();
    if (!db.isOpen())
        return false;

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM covers WHERE key = ?"));
    query.addBindValue(keyFor(videoPath, fileSize, lastModified));
    return query.exec() && query.next();
}

QImage ThumbnailCache::load(const QString &videoPath, qint64 fileSize, qint64 lastModified) const
{
    if (videoPath.isEmpty())
        return QImage();

    QSqlDatabase db = connection();
    if (!db.isOpen())
        return QImage();

    const QString key = keyFor(videoPath, fileSize, lastModified);

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT data FROM covers WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec() || !query.next())
        return QImage();

    QImage image;
    if (!image.loadFromData(query.value(0).toByteArray()))
        return QImage();

    // Touch the last-used time so LRU pruning keeps frequently used covers.
    QSqlQuery touch(db);
    touch.prepare(QStringLiteral("UPDATE covers SET last_used = ? WHERE key = ?"));
    touch.addBindValue(QDateTime::currentMSecsSinceEpoch());
    touch.addBindValue(key);
    touch.exec();

    return image;
}

bool ThumbnailCache::store(const QString &videoPath, qint64 fileSize, qint64 lastModified, const QImage &image)
{
    if (videoPath.isEmpty() || image.isNull())
        return false;

    QByteArray blob;
    QBuffer buffer(&blob);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPG", 88)) {
        qCWarning(lcThumbCache) << "failed to encode thumbnail";
        return false;
    }

    QSqlDatabase db = connection();
    if (!db.isOpen())
        return false;

    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT INTO covers (key, data, last_used) VALUES (?, ?, ?) "
                                 "ON CONFLICT(key) DO UPDATE SET data = excluded.data, "
                                 "last_used = excluded.last_used"));
    query.addBindValue(keyFor(videoPath, fileSize, lastModified));
    query.addBindValue(blob);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!query.exec()) {
        qCWarning(lcThumbCache) << "failed to store cover" << query.lastError().text();
        return false;
    }

    emit changed();
    return true;
}

qint64 ThumbnailCache::totalBytes() const
{
    QSqlDatabase db = connection();
    if (!db.isOpen())
        return 0;

    QSqlQuery query(db);
    if (query.exec(QStringLiteral("SELECT COALESCE(SUM(length(data)), 0) FROM covers")) && query.next())
        return query.value(0).toLongLong();
    return 0;
}

int ThumbnailCache::entryCount() const
{
    QSqlDatabase db = connection();
    if (!db.isOpen())
        return 0;

    QSqlQuery query(db);
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM covers")) && query.next())
        return query.value(0).toInt();
    return 0;
}

void ThumbnailCache::prune(qint64 maxBytes, int maxEntries)
{
    QSqlDatabase db = connection();
    if (!db.isOpen())
        return;

    qint64 bytes = totalBytes();
    int entries = entryCount();
    if (bytes <= maxBytes && entries <= maxEntries)
        return;

    // Collect the oldest entries first so the SELECT result can't be invalidated
    // by the deletes that follow.
    QSqlQuery select(db);
    select.exec(QStringLiteral("SELECT key, length(data) FROM covers ORDER BY last_used ASC"));

    QStringList doomed;
    while (select.next() && (bytes > maxBytes || entries > maxEntries)) {
        doomed << select.value(0).toString();
        bytes -= select.value(1).toLongLong();
        --entries;
    }

    if (doomed.isEmpty())
        return;

    db.transaction();
    QSqlQuery del(db);
    del.prepare(QStringLiteral("DELETE FROM covers WHERE key = ?"));
    int removed = 0;
    for (const QString &key : doomed) {
        del.addBindValue(key);
        if (del.exec())
            ++removed;
    }
    db.commit();

    if (removed > 0) {
        qCInfo(lcThumbCache) << "pruned" << removed << "cached covers";
        emit changed();
    }
}

int ThumbnailCache::clear()
{
    QSqlDatabase db = connection();
    if (!db.isOpen())
        return 0;

    const int removed = entryCount();
    QSqlQuery query(db);
    query.exec(QStringLiteral("DELETE FROM covers"));

    if (removed > 0) {
        emit changed();
        qCInfo(lcThumbCache) << "cleared" << removed << "cached covers";
    }
    return removed;
}
