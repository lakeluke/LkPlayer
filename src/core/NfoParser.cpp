#include "core/NfoParser.h"

#include <QFile>
#include <QXmlStreamReader>

namespace {

QString cleaned(const QString &text)
{
    return text.simplified();
}

void appendUnique(QStringList *list, const QString &value)
{
    const QString trimmed = cleaned(value);
    if (trimmed.isEmpty())
        return;
    for (const QString &existing : *list) {
        if (existing.compare(trimmed, Qt::CaseInsensitive) == 0)
            return;
    }
    list->append(trimmed);
}

struct ParseOutcome
{
    QStringList openElements; //!< elements still open when parsing stopped
    QString errorString;
    int lineNumber = 0;
    bool malformed = false;
};

/**
 * Single pass over the document. Never throws: a damaged document yields the
 * fields that were read before the problem plus a description of the problem.
 */
ParseOutcome parseDocument(const QByteArray &xml, NfoData *out)
{
    ParseOutcome outcome;

    QXmlStreamReader reader(xml);
    QStringList stack;
    QString pending;
    QString currentActorName;
    QString currentActorRole;
    bool insideActor = false;

    const auto assign = [&](const QString &name, int depth, const QString &text) {
        if (insideActor) {
            if (name == QLatin1String("name"))
                currentActorName = text;
            else if (name == QLatin1String("role"))
                currentActorRole = text;
            else if (name == QLatin1String("actor")) {
                insideActor = false;
                QString label = currentActorName;
                if (!currentActorRole.isEmpty())
                    label += QStringLiteral(" / ") + currentActorRole;
                appendUnique(&out->actors, label);
            }
            return;
        }

        if (text.isEmpty())
            return;

        // Direct children of the nfo root.
        if (depth == 1) {
            if (name == QLatin1String("title"))
                out->title = text;
            else if (name == QLatin1String("originaltitle"))
                out->originalTitle = text;
            else if (name == QLatin1String("sorttitle") && out->title.isEmpty())
                out->title = text;
            else if (name == QLatin1String("plot"))
                out->plot = text;
            else if (name == QLatin1String("outline"))
                out->outline = text;
            else if (name == QLatin1String("tagline"))
                out->tagline = text;
            else if (name == QLatin1String("year"))
                out->year = text;
            else if (name == QLatin1String("premiered") || name == QLatin1String("releasedate"))
                out->premiered = text;
            else if (name == QLatin1String("studio"))
                out->studio = out->studio.isEmpty() ? text : out->studio;
            else if (name == QLatin1String("director"))
                out->director = text;
            else if (name == QLatin1String("credits"))
                out->credits = text;
            else if (name == QLatin1String("mpaa"))
                out->mpaa = text;
            else if (name == QLatin1String("genre"))
                appendUnique(&out->genres, text);
            else if (name == QLatin1String("tag"))
                appendUnique(&out->tags, text);
            else if (name == QLatin1String("country"))
                appendUnique(&out->countries, text);
            else if (name == QLatin1String("writer"))
                appendUnique(&out->writers, text);
            else if (name == QLatin1String("runtime")) {
                bool ok = false;
                const double minutes = text.toDouble(&ok);
                if (ok && minutes > 0.0)
                    out->runtimeMinutes = static_cast<int>(minutes + 0.5);
            } else if (name == QLatin1String("rating")) {
                bool ok = false;
                const double value = text.toDouble(&ok);
                if (ok)
                    out->rating = value;
            } else if (name == QLatin1String("thumb")) {
                if (out->posterThumb.isEmpty())
                    out->posterThumb = text;
            } else if (name == QLatin1String("id")) {
                if (out->setId.isEmpty())
                    out->setId = text;
            }
            return;
        }

        if (depth == 2) {
            if (name == QLatin1String("thumb")) {
                if (stack.contains(QStringLiteral("fanart")) && out->fanartThumb.isEmpty())
                    out->fanartThumb = text;
            } else if (name == QLatin1String("name") && stack.contains(QStringLiteral("set"))) {
                out->setName = text;
            } else if (name == QLatin1String("value") && stack.contains(QStringLiteral("rating"))) {
                bool ok = false;
                const double value = text.toDouble(&ok);
                if (ok && out->rating <= 0.0)
                    out->rating = value;
            }
        }
    };

    while (!reader.atEnd()) {
        const QXmlStreamReader::TokenType token = reader.readNext();
        if (reader.hasError())
            break;

        if (token == QXmlStreamReader::StartElement) {
            const QString name = reader.name().toString().toLower();
            stack.append(name);
            pending.clear();
            if (name == QLatin1String("actor")) {
                insideActor = true;
                currentActorName.clear();
                currentActorRole.clear();
            }
            continue;
        }

        if (token == QXmlStreamReader::Characters) {
            if (!stack.isEmpty())
                pending.append(reader.text().toString());
            continue;
        }

        if (token != QXmlStreamReader::EndElement)
            continue;

        const QString name = stack.isEmpty() ? QString() : stack.takeLast();
        const QString text = cleaned(pending);
        pending.clear();
        if (!name.isEmpty())
            assign(name, stack.size(), text); // stack.size() is the depth of `name`
    }

    if (reader.hasError()) {
        outcome.malformed = true;
        outcome.lineNumber = reader.lineNumber();
        outcome.errorString = QStringLiteral("nfo 解析在第 %1 行出错：%2")
                                  .arg(reader.lineNumber())
                                  .arg(reader.errorString());
        outcome.openElements = stack;
    }

    return outcome;
}

} // namespace

NfoData NfoParser::parseFile(const QString &path, QString *error)
{
    NfoData data;
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("empty path");
        return data;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("无法打开文件：%1").arg(path);
        return data;
    }

    const QByteArray raw = file.readAll();
    file.close();

    QString parseError;
    parseXml(raw, &data, &parseError);
    if (error)
        *error = parseError;
    return data;
}

bool NfoParser::parseXml(const QByteArray &xml, NfoData *out, QString *error)
{
    if (!out)
        return false;

    const ParseOutcome outcome = parseDocument(xml, out);

    if (!outcome.malformed) {
        if (error)
            error->clear();
        // Nothing recognisable inside a well formed document: report it so the
        // caller keeps the file name based title.
        return !out->isEmpty();
    }

    if (error)
        *error = outcome.errorString;

    // A truncated file (an interrupted scraper, a killed download) can often be
    // recovered by closing whatever was still open and parsing again.
    if (!outcome.openElements.isEmpty()) {
        QByteArray repaired = xml;
        for (int i = outcome.openElements.size() - 1; i >= 0; --i)
            repaired += "</" + outcome.openElements.at(i).toUtf8() + ">";

        NfoData recovered;
        const ParseOutcome retry = parseDocument(repaired, &recovered);
        if (!retry.malformed && !recovered.isEmpty())
            *out = recovered;
    }

    return true;
}

QStringList NfoParser::normalizedTags(const NfoData &data)
{
    QStringList result;
    for (const QString &genre : data.genres)
        appendUnique(&result, genre);
    for (const QString &tag : data.tags)
        appendUnique(&result, tag);
    return result;
}
