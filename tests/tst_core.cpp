#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "core/LibraryIndex.h"
#include "core/LibraryScanner.h"
#include "core/NfoParser.h"
#include "core/PathUtils.h"

namespace {

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(content);
    file.close();
    return true;
}

const char *kMovieNfo = R"(<?xml version="1.0" encoding="UTF-8"?>
<movie>
  <title>PPC-001 Title</title>
  <originaltitle>Original PPC-001</originaltitle>
  <year>2024</year>
  <runtime>96</runtime>
  <rating>7.8</rating>
  <plot>一段用于单元测试的简介。</plot>
  <genre>Drama</genre>
  <genre>Sci-Fi</genre>
  <tag>4K</tag>
  <tag>HDR</tag>
  <actor><name>Alice</name><role>Lead</role></actor>
  <actor><name>Bob</name><role>Support</role></actor>
  <studio>LkStudio</studio>
</movie>
)";

} // namespace

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    // ---------------------------------------------------------------- nfo ---
    void parsesCompleteNfo()
    {
        NfoData data;
        QString error;
        QVERIFY(NfoParser::parseXml(QByteArray(kMovieNfo), &data, &error));
        QVERIFY2(error.isEmpty(), qPrintable(error));

        QCOMPARE(data.title, QStringLiteral("PPC-001 Title"));
        QCOMPARE(data.year, QStringLiteral("2024"));
        QCOMPARE(data.runtimeMinutes, 96);
        QVERIFY(qAbs(data.rating - 7.8) < 0.001);
        QCOMPARE(data.genres, QStringList({ "Drama", "Sci-Fi" }));
        QCOMPARE(data.tags.size(), 2);
        QCOMPARE(data.actors.size(), 2);
        QVERIFY(data.actors.first().startsWith(QLatin1String("Alice")));
        QCOMPARE(data.studio, QStringLiteral("LkStudio"));

        const QStringList normalized = NfoParser::normalizedTags(data);
        QCOMPARE(normalized.size(), 4);
        QVERIFY(normalized.contains(QStringLiteral("HDR")));
    }

    void toleratesBrokenXml()
    {
        NfoData data;
        QString error;
        NfoParser::parseXml(QByteArray("<movie><title>Half"), &data, &error);
        QCOMPARE(data.title, QStringLiteral("Half"));
        QVERIFY(!error.isEmpty());
    }

    void handlesMissingFields()
    {
        NfoData data;
        QVERIFY(NfoParser::parseXml(QByteArray("<movie><title>Only</title></movie>"), &data, nullptr));
        QCOMPARE(data.title, QStringLiteral("Only"));
        QVERIFY(data.genres.isEmpty());
        QCOMPARE(data.runtimeMinutes, 0);
    }

    // ----------------------------------------------------------- utilities ---
    void detectsMediaTypes()
    {
        QVERIFY(PathUtils::isVideoFile(QStringLiteral("a.MP4")));
        QVERIFY(PathUtils::isVideoFile(QStringLiteral("a.mkv")));
        QVERIFY(!PathUtils::isVideoFile(QStringLiteral("a.txt")));
        QVERIFY(PathUtils::isSubtitleFile(QStringLiteral("a.zh.srt")));
        QVERIFY(PathUtils::isImageFile(QStringLiteral("poster.webp")));
        QCOMPARE(PathUtils::baseName(QStringLiteral("PPC-001-title.mp4")), QStringLiteral("PPC-001-title"));
        QVERIFY(PathUtils::isIgnoredDirectory(QStringLiteral(".cache")));
        QVERIFY(PathUtils::isIgnoredDirectory(QStringLiteral("@eaDir")));
    }

    void encodesIdentifiersRoundTrip()
    {
        const QString path = QStringLiteral("V:/媒体库/PPC-001/PPC-001-title.mp4");
        QCOMPARE(PathUtils::decodeId(PathUtils::encodeId(path)), path);
    }

    void formatsDurations()
    {
        QCOMPARE(PathUtils::formatDuration(0), QStringLiteral("--:--"));
        QCOMPARE(PathUtils::formatDuration(95), QStringLiteral("1:35"));
        QCOMPARE(PathUtils::formatDuration(3725), QStringLiteral("1:02:05"));
    }

    // ------------------------------------------------------------- scanner ---
    void scansHierarchyAndSidecars()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const QString root = temp.path();
        const QString leaf = root + "/Scene/Act1/PPC-001";
        QVERIFY(QDir().mkpath(leaf));

        const QString video = leaf + "/PPC-001-title.mp4";
        QVERIFY(writeFile(video, "fake video"));
        QVERIFY(writeFile(leaf + "/PPC-001-title.nfo", kMovieNfo));
        QVERIFY(writeFile(leaf + "/PPC-001-title.srt", "1\n00:00:01,000 --> 00:00:02,000\nhi\n"));
        QVERIFY(writeFile(leaf + "/PPC-001-title-still01.jpg", "jpeg"));
        QVERIFY(writeFile(leaf + "/poster.jpg", "jpeg"));
        QVERIFY(writeFile(leaf + "/notes.txt", "ignore me"));

        // A second level so the folder tree has a real branch.
        const QString other = root + "/Scene/Act2/PPC-002";
        QVERIFY(QDir().mkpath(other));
        QVERIFY(writeFile(other + "/PPC-002-title.mp4", "fake video 2"));

        LibraryScanner scanner;
        QSignalSpy itemsSpy(&scanner, &LibraryScanner::itemsReady);
        QSignalSpy structureSpy(&scanner, &LibraryScanner::structureReady);
        QSignalSpy finishedSpy(&scanner, &LibraryScanner::scanFinished);

        scanner.setRootPath(root);
        scanner.start();

        QVERIFY(finishedSpy.wait(20000));
        QVERIFY(itemsSpy.count() > 0);
        // A fresh scan publishes the folder tree incrementally as it grows, so
        // the complete structure is the last emission, not the only one.
        QVERIFY(structureSpy.count() >= 1);

        MediaItemList items;
        for (const QList<QVariant> &args : itemsSpy)
            items += args.at(0).value<MediaItemList>();
        QCOMPARE(items.size(), 2);

        const MediaItem *target = nullptr;
        for (const MediaItem &item : items) {
            if (item.fileName == QLatin1String("PPC-001-title.mp4"))
                target = &item;
        }
        QVERIFY(target != nullptr);
        QCOMPARE(target->title, QStringLiteral("PPC-001 Title"));
        QCOMPARE(target->relativeFolder, QStringLiteral("Scene/Act1/PPC-001"));
        QVERIFY(target->posterPath.endsWith(QLatin1String("poster.jpg")));
        QCOMPARE(target->subtitles.size(), 1);
        QCOMPARE(target->stillImages.size(), 1);
        QVERIFY(target->tags.contains(QStringLiteral("4K")));
        QVERIFY(qAbs(target->duration - 96 * 60.0) < 0.001);

        const FolderNodeList folders = structureSpy.last().at(0).value<FolderNodeList>();
        QHash<QString, FolderNode> byPath;
        for (const FolderNode &node : folders)
            byPath.insert(node.path, node);

        const FolderNode rootNode = byPath.value(QDir::cleanPath(root));
        QCOMPARE(rootNode.directVideoCount, 0);
        QCOMPARE(rootNode.totalVideoCount, 2);
        QCOMPARE(rootNode.childPaths.size(), 1);

        const FolderNode leafNode = byPath.value(QDir::cleanPath(leaf));
        QCOMPARE(leafNode.directVideoCount, 1);
        QCOMPARE(leafNode.totalVideoCount, 1);
        QCOMPARE(leafNode.depth, 3);
    }

    void ignoresIgnoredDirectories()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        QVERIFY(QDir().mkpath(temp.path() + "/.hidden"));
        QVERIFY(QDir().mkpath(temp.path() + "/visible"));
        QVERIFY(writeFile(temp.path() + "/.hidden/nope.mp4", "x"));
        QVERIFY(writeFile(temp.path() + "/visible/yes.mp4", "x"));

        LibraryScanner scanner;
        QSignalSpy itemsSpy(&scanner, &LibraryScanner::itemsReady);
        QSignalSpy finishedSpy(&scanner, &LibraryScanner::scanFinished);
        scanner.setRootPath(temp.path());
        scanner.start();
        QVERIFY(finishedSpy.wait(15000));

        MediaItemList items;
        for (const QList<QVariant> &args : itemsSpy)
            items += args.at(0).value<MediaItemList>();
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.first().fileName, QStringLiteral("yes.mp4"));
    }

    // --------------------------------------------------------------- index ---
    void indexSurvivesRoundTrip()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString file = temp.path() + "/index.bin";

        LibraryIndex index;
        index.setRootPath(QStringLiteral("/lib/root"));

        LibraryFolderSnapshot snapshot;
        snapshot.node.path = QStringLiteral("/lib/root/Scene");
        snapshot.node.name = QStringLiteral("Scene");
        snapshot.node.depth = 1;
        snapshot.node.childPaths = { QStringLiteral("/lib/root/Scene/A") };
        snapshot.node.directVideoCount = 1;
        snapshot.node.totalVideoCount = 3;
        snapshot.node.tagCounts = { { QStringLiteral("Drama"), 2 } };
        snapshot.directTagCounts = { { QStringLiteral("Drama"), 1 } };
        snapshot.modifiedMs = 123456789;

        MediaItem item;
        item.filePath = QStringLiteral("/lib/root/Scene/a.mp4");
        item.fileName = QStringLiteral("a.mp4");
        item.folderPath = QStringLiteral("/lib/root/Scene");
        item.title = QStringLiteral("A");
        item.tags = { QStringLiteral("Drama") };
        item.duration = 60.0;
        item.fileSize = 42;
        item.lastModified = 999;
        snapshot.items.append(item);
        index.insert(snapshot);

        QVERIFY(index.save(file));

        LibraryIndex loaded;
        QVERIFY(LibraryIndex::load(file, QStringLiteral("/lib/root"), &loaded));
        QCOMPARE(loaded.folderCount(), 1);

        const LibraryFolderSnapshot *restored = loaded.find(QStringLiteral("/lib/root/Scene"));
        QVERIFY(restored != nullptr);
        QCOMPARE(restored->modifiedMs, snapshot.modifiedMs);
        QCOMPARE(restored->node.totalVideoCount, 3);
        QCOMPARE(restored->node.tagCounts.value(QStringLiteral("Drama")), 2);
        QCOMPARE(restored->directTagCounts.value(QStringLiteral("Drama")), 1);
        QCOMPARE(restored->items.size(), 1);
        QCOMPARE(restored->items.first().title, QStringLiteral("A"));
        QCOMPARE(restored->items.first().duration, 60.0);

        // An index written for one library must never be applied to another.
        LibraryIndex foreign;
        QVERIFY(!LibraryIndex::load(file, QStringLiteral("/other/root"), &foreign));
    }

    void incrementalScanOnlyReportsChangedFolders()
    {
        QTemporaryDir library;
        QTemporaryDir cache;
        QVERIFY(library.isValid());
        QVERIFY(cache.isValid());

        const QString root = library.path();
        const QString folder = root + "/Scene/PPC-001";
        QVERIFY(QDir().mkpath(folder));
        QVERIFY(writeFile(folder + "/PPC-001-title.mp4", "fake video"));

        const QString indexFile = cache.path() + "/index.bin";

        // Let the freshly written directory timestamp settle first, otherwise
        // the first scan would record a value the file system still updates.
        QTest::qWait(1200);

        {
            LibraryScanner scanner;
            QSignalSpy finishedSpy(&scanner, &LibraryScanner::scanFinished);
            scanner.setRootPath(root);
            scanner.setIndexFile(indexFile);
            scanner.start();
            QVERIFY(finishedSpy.wait(20000));
        }
        QVERIFY(QFile::exists(indexFile));

        // Second pass, nothing touched on disk: the cached rows are published
        // and no folder is re-read.
        {
            LibraryScanner scanner;
            QSignalSpy itemsSpy(&scanner, &LibraryScanner::itemsReady);
            QSignalSpy deltaSpy(&scanner, &LibraryScanner::folderItemsChanged);
            QSignalSpy finishedSpy(&scanner, &LibraryScanner::scanFinished);
            scanner.setRootPath(root);
            scanner.setIndexFile(indexFile);
            scanner.start();
            QVERIFY(finishedSpy.wait(20000));

            MediaItemList published;
            for (const QList<QVariant> &args : itemsSpy)
                published += args.at(0).value<MediaItemList>();
            QCOMPARE(published.size(), 1);
            QCOMPARE(deltaSpy.count(), 0);
        }

        // Third pass after adding a file: only that folder reports a change.
        // Windows only flushes a directory timestamp lazily, so wait until the
        // change is actually readable before asking for the incremental scan.
        const QDateTime beforeSecondFile = QFileInfo(folder).lastModified();
        QVERIFY(writeFile(folder + "/PPC-001-second.mp4", "another video"));
        QVERIFY(QTest::qWaitFor(
            [folder, beforeSecondFile]() {
                return QFileInfo(folder).lastModified() != beforeSecondFile;
            },
            5000));

        LibraryScanner scanner;
        QSignalSpy deltaSpy(&scanner, &LibraryScanner::folderItemsChanged);
        QSignalSpy finishedSpy(&scanner, &LibraryScanner::scanFinished);
        scanner.setRootPath(root);
        scanner.setIndexFile(indexFile);
        scanner.start();
        QVERIFY(finishedSpy.wait(20000));

        QCOMPARE(deltaSpy.count(), 1);
        QCOMPARE(deltaSpy.at(0).at(0).toString(), QDir::cleanPath(folder));
        QCOMPARE(deltaSpy.at(0).at(1).value<MediaItemList>().size(), 2);
    }

    void fullScanIgnoresTheIndex()
    {
        QTemporaryDir library;
        QTemporaryDir cache;
        QVERIFY(library.isValid());
        QVERIFY(cache.isValid());

        const QString root = library.path();
        QVERIFY(QDir().mkpath(root + "/A"));
        QVERIFY(writeFile(root + "/A/one.mp4", "x"));
        const QString indexFile = cache.path() + "/index.bin";

        {
            LibraryScanner scanner;
            QSignalSpy finishedSpy(&scanner, &LibraryScanner::scanFinished);
            scanner.setRootPath(root);
            scanner.setIndexFile(indexFile);
            scanner.start();
            QVERIFY(finishedSpy.wait(20000));
        }

        LibraryScanner scanner;
        QSignalSpy itemsSpy(&scanner, &LibraryScanner::itemsReady);
        QSignalSpy deltaSpy(&scanner, &LibraryScanner::folderItemsChanged);
        QSignalSpy structureSpy(&scanner, &LibraryScanner::structureReady);
        QSignalSpy finishedSpy(&scanner, &LibraryScanner::scanFinished);
        scanner.setRootPath(root);
        scanner.setIndexFile(indexFile);
        scanner.startWithMode(LibraryScanner::Mode::Full);
        QVERIFY(finishedSpy.wait(20000));

        MediaItemList items;
        for (const QList<QVariant> &args : itemsSpy)
            items += args.at(0).value<MediaItemList>();
        QCOMPARE(items.size(), 1);
        QCOMPARE(deltaSpy.count(), 0);
        QVERIFY(structureSpy.count() >= 1);
    }
};

QTEST_MAIN(TestCore)
#include "tst_core.moc"
