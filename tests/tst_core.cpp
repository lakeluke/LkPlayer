#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

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
        QCOMPARE(structureSpy.count(), 1);

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

        const FolderNodeList folders = structureSpy.at(0).at(0).value<FolderNodeList>();
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
};

QTEST_MAIN(TestCore)
#include "tst_core.moc"
