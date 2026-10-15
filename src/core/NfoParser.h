#pragma once

#include <QString>
#include <QStringList>

/** Everything LkPlayer cares about inside a Kodi style `.nfo` file. */
struct NfoData
{
    QString title;
    QString originalTitle;
    QString plot;
    QString outline;
    QString tagline;
    QString year;
    QString premiered;
    QString studio;
    QString director;
    QString credits;
    QString mpaa;
    QString setId;
    QString setName;
    QString posterThumb;
    QString fanartThumb;
    int runtimeMinutes = 0;
    double rating = 0.0;
    QStringList genres;
    QStringList tags;
    QStringList actors;
    QStringList countries;
    QStringList writers;

    bool isEmpty() const
    {
        return title.isEmpty() && plot.isEmpty() && genres.isEmpty() && tags.isEmpty() && actors.isEmpty();
    }
};

/**
 * Tolerant Kodi/Kodi-like nfo reader. Malformed XML never throws: the parser
 * keeps whatever it managed to read and reports the problem through `error`.
 */
class NfoParser
{
public:
    static NfoData parseFile(const QString &path, QString *error = nullptr);
    static bool parseXml(const QByteArray &xml, NfoData *out, QString *error = nullptr);

    /** Merges genres and tags into one normalised, de-duplicated tag list. */
    static QStringList normalizedTags(const NfoData &data);
};
