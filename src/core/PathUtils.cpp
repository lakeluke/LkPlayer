#include "core/PathUtils.h"

#include <QDir>
#include <QFileInfo>

namespace PathUtils {
namespace {

QString absoluteChild(const QString &dirPath, const QString &fileName)
{
    return QDir(dirPath).absoluteFilePath(fileName);
}

QString findImageByBase(const DirIndex &index, const QStringList &bases)
{
    static const QStringList exts = imageExtensions();
    for (const QString &base : bases) {
        if (base.isEmpty())
            continue;
        for (const QString &ext : exts) {
            const QString real = index.resolve(base + QLatin1Char('.') + ext);
            if (!real.isEmpty())
                return absoluteChild(index.path(), real);
        }
    }
    return QString();
}

} // namespace

const QStringList &videoExtensions()
{
    static const QStringList exts = {
        QStringLiteral("mp4"),  QStringLiteral("mkv"),  QStringLiteral("avi"),
        QStringLiteral("mov"),  QStringLiteral("m4v"),  QStringLiteral("webm"),
        QStringLiteral("wmv"),  QStringLiteral("flv"),  QStringLiteral("ts"),
        QStringLiteral("m2ts"), QStringLiteral("mts"),  QStringLiteral("mpg"),
        QStringLiteral("mpeg"), QStringLiteral("vob"),  QStringLiteral("rmvb"),
        QStringLiteral("rm"),   QStringLiteral("3gp"),  QStringLiteral("ogv"),
        QStringLiteral("asf"),  QStringLiteral("f4v"),  QStringLiteral("m2v"),
        QStringLiteral("divx"), QStringLiteral("mxf"),
    };
    return exts;
}

const QStringList &subtitleExtensions()
{
    static const QStringList exts = {
        QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("ssa"),
        QStringLiteral("sub"), QStringLiteral("vtt"), QStringLiteral("smi"),
        QStringLiteral("idx"), QStringLiteral("sup"),
    };
    return exts;
}

const QStringList &imageExtensions()
{
    static const QStringList exts = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("webp"), QStringLiteral("bmp"), QStringLiteral("gif"),
    };
    return exts;
}

bool isVideoFile(const QString &fileName)
{
    const QString suffix = QFileInfo(fileName).suffix();
    return !suffix.isEmpty() && videoExtensions().contains(suffix, Qt::CaseInsensitive);
}

bool isSubtitleFile(const QString &fileName)
{
    const QString suffix = QFileInfo(fileName).suffix();
    return !suffix.isEmpty() && subtitleExtensions().contains(suffix, Qt::CaseInsensitive);
}

bool isImageFile(const QString &fileName)
{
    const QString suffix = QFileInfo(fileName).suffix();
    return !suffix.isEmpty() && imageExtensions().contains(suffix, Qt::CaseInsensitive);
}

QString baseName(const QString &fileName)
{
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    return dot <= 0 ? fileName : fileName.left(dot);
}

bool isIgnoredDirectory(const QString &folderName)
{
    static const QStringList ignored = {
        QStringLiteral(".lkcache"), QStringLiteral("@eadir"), QStringLiteral("$recycle.bin"),
        QStringLiteral("system volume information"), QStringLiteral(".git"),
        QStringLiteral(".svn"), QStringLiteral("node_modules"), QStringLiteral(".trash"),
        QStringLiteral("lost+found"), QStringLiteral("__macosx"),
    };
    if (folderName.startsWith(QLatin1Char('.')))
        return true;
    return ignored.contains(folderName.toLower());
}

void DirIndex::build(const QString &dirPath)
{
    m_path.clear();
    m_files.clear();
    m_subDirs.clear();
    m_lookup.clear();

    QDir dir(dirPath);
    if (!dir.exists())
        return;

    m_path = dir.absolutePath();

    const QFileInfoList entries =
        dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name);
    for (const QFileInfo &info : entries) {
        const QString name = info.fileName();
        if (info.isDir()) {
            m_subDirs.append(name);
        } else {
            m_files.append(name);
            m_lookup.insert(name.toLower(), name);
        }
    }
}

QString DirIndex::resolve(const QString &fileName) const
{
    return m_lookup.value(fileName.toLower());
}

bool DirIndex::hasFile(const QString &fileName) const
{
    return m_lookup.contains(fileName.toLower());
}

QString findPoster(const DirIndex &index, const QString &videoBase)
{
    QStringList bases;
    bases << videoBase
          << videoBase + QStringLiteral("-poster")
          << videoBase + QStringLiteral(".poster")
          << videoBase + QStringLiteral("-cover")
          << videoBase + QStringLiteral("-thumb")
          << videoBase + QStringLiteral("-folder");
    return findImageByBase(index, bases);
}

QString findFanart(const DirIndex &index, const QString &videoBase, const QString &posterPath)
{
    QStringList bases;
    bases << videoBase + QStringLiteral("-fanart")
          << videoBase + QStringLiteral("-backdrop")
          << videoBase + QStringLiteral("-background");
    const QString specific = findImageByBase(index, bases);
    if (!specific.isEmpty())
        return specific;

    static const QStringList generic = {
        QStringLiteral("fanart"), QStringLiteral("backdrop"),
        QStringLiteral("background"), QStringLiteral("landscape"),
    };
    const QString found = findImageByBase(index, generic);
    if (!found.isEmpty() && found.compare(posterPath, Qt::CaseInsensitive) != 0)
        return found;
    return QString();
}

QString findNfo(const DirIndex &index, const QString &videoBase, const QString &folderName)
{
    QStringList bases;
    bases << videoBase;
    if (!folderName.isEmpty() && folderName.compare(videoBase, Qt::CaseInsensitive) != 0)
        bases << folderName;
    bases << QStringLiteral("movie")
          << QStringLiteral("tvshow")
          << QStringLiteral("episode")
          << QStringLiteral("album");

    for (const QString &base : bases) {
        const QString real = index.resolve(base + QStringLiteral(".nfo"));
        if (!real.isEmpty())
            return absoluteChild(index.path(), real);
    }
    return QString();
}

QStringList findSubtitles(const DirIndex &index, const QString &videoBase)
{
    QStringList result;
    const QString lowerBase = videoBase.toLower();
    for (const QString &file : index.files()) {
        if (!isSubtitleFile(file))
            continue;
        const QString lower = file.toLower();
        if (lower.size() <= lowerBase.size())
            continue;
        if (!lower.startsWith(lowerBase))
            continue;
        const QChar separator = lower.at(lowerBase.size());
        if (separator != QLatin1Char('.') && separator != QLatin1Char('-') && separator != QLatin1Char('_'))
            continue;
        result.append(absoluteChild(index.path(), file));
    }
    result.sort();
    return result;
}

QStringList findStillImages(const DirIndex &index, const QString &videoBase, const QString &posterPath)
{
    QStringList result;
    const QString lowerBase = videoBase.toLower();
    const QString lowerPoster = posterPath.toLower();

    for (const QString &file : index.files()) {
        if (!isImageFile(file))
            continue;
        const QString lower = file.toLower();
        if (lower.size() <= lowerBase.size() || !lower.startsWith(lowerBase))
            continue;
        const QChar separator = lower.at(lowerBase.size());
        if (separator != QLatin1Char('.') && separator != QLatin1Char('-') && separator != QLatin1Char('_'))
            continue;
        const QString abs = absoluteChild(index.path(), file);
        if (!lowerPoster.isEmpty() && abs.toLower() == lowerPoster)
            continue;
        result.append(abs);
    }

    static const QStringList extraDirs = {
        QStringLiteral("extrafanart"), QStringLiteral("stills"),
        QStringLiteral("screenshots"), QStringLiteral("thumbs"),
        QStringLiteral("backdrops"),
    };
    for (const QString &sub : index.subDirs()) {
        if (!extraDirs.contains(sub.toLower()))
            continue;
        const QDir dir(absoluteChild(index.path(), sub));
        const QStringList files = dir.entryList(QDir::Files | QDir::Readable, QDir::Name);
        for (const QString &file : files) {
            if (isImageFile(file))
                result.append(dir.absoluteFilePath(file));
        }
    }

    result.removeDuplicates();
    result.sort();
    return result;
}

QString findFolderPoster(const DirIndex &index, const QString &folderName)
{
    if (!index.isValid())
        return QString();

    QStringList bases;
    if (!folderName.isEmpty())
        bases << folderName;
    bases << QStringLiteral("poster")
          << QStringLiteral("folder")
          << QStringLiteral("cover")
          << QStringLiteral("default")
          << QStringLiteral("thumb");
    return findImageByBase(index, bases);
}

QString findFolderPoster(const QString &folderPath, const QString &folderName)
{
    DirIndex index;
    index.build(folderPath);
    return findFolderPoster(index, folderName);
}

QString formatDuration(double seconds)
{
    if (seconds <= 0.0)
        return QStringLiteral("--:--");
    const int total = static_cast<int>(seconds + 0.5);
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int secs = total % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(minutes).arg(secs, 2, 10, QLatin1Char('0'));
}

QString formatSize(qint64 bytes)
{
    static const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', unit == 0 ? 0 : 1).arg(QLatin1String(units[unit]));
}

QString encodeId(const QString &text)
{
    return QString::fromLatin1(text.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString decodeId(const QString &text)
{
    return QString::fromUtf8(QByteArray::fromBase64(text.toLatin1(), QByteArray::Base64UrlEncoding));
}

QString resolveToolIn(const QString &dir, const QString &toolBaseName)
{
    if (dir.isEmpty())
        return QString();

    QDir directory(dir);
    if (!directory.exists())
        return QString();

    const QStringList candidates = {
        toolBaseName + QStringLiteral(".exe"),
        toolBaseName,
        toolBaseName + QStringLiteral(".cmd"),
        toolBaseName + QStringLiteral(".bat"),
    };
    for (const QString &candidate : candidates) {
        const QFileInfo info(directory.absoluteFilePath(candidate));
        if (info.exists() && info.isFile())
            return info.absoluteFilePath();
    }
    return QString();
}

bool validateToolDir(const QString &dir, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    if (dir.trimmed().isEmpty())
        return fail(QStringLiteral("尚未设置程序目录"));

    const QFileInfo info(dir);
    if (!info.exists() || !info.isDir())
        return fail(QStringLiteral("目录不存在：%1").arg(dir));

    if (resolveToolIn(dir, QStringLiteral("ffmpeg")).isEmpty())
        return fail(QStringLiteral("目录中未找到 ffmpeg 可执行文件"));

    if (resolveToolIn(dir, QStringLiteral("ffplay")).isEmpty())
        return fail(QStringLiteral("目录中未找到 ffplay 可执行文件"));

    if (error)
        error->clear();
    return true;
}

} // namespace PathUtils
