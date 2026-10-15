#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTextStream>

#include "app/AppController.h"
#include "app/AppPaths.h"
#include "thumb/ThumbnailProvider.h"
#include "thumb/ThumbnailService.h"

namespace {

/// Builds the application icon at runtime so the project needs no binary assets.
QIcon buildAppIcon()
{
    constexpr int kSize = 256;
    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient background(0, 0, kSize, kSize);
    background.setColorAt(0.0, QColor(QStringLiteral("#1E2029")));
    background.setColorAt(1.0, QColor(QStringLiteral("#0E0F13")));

    QPainterPath squircle;
    squircle.addRoundedRect(QRectF(12, 12, kSize - 24, kSize - 24), 58, 58);
    painter.fillPath(squircle, background);

    QLinearGradient accent(0, 0, kSize, kSize);
    accent.setColorAt(0.0, QColor(QStringLiteral("#A78BFA")));
    accent.setColorAt(1.0, QColor(QStringLiteral("#5B3DF5")));

    painter.setPen(QPen(accent, 12));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(kSize / 2.0, kSize / 2.0), 74, 74);

    QPainterPath triangle;
    triangle.moveTo(kSize * 0.42, kSize * 0.33);
    triangle.lineTo(kSize * 0.72, kSize * 0.50);
    triangle.lineTo(kSize * 0.42, kSize * 0.67);
    triangle.closeSubpath();
    painter.fillPath(triangle, accent);

    painter.end();
    return QIcon(pixmap);
}

/// Writes log output to a file so the WIN32 (console-less) build stays debuggable.
void installLogHandler()
{
    const QString path = QDir(AppPaths::logDir()).absoluteFilePath(QStringLiteral("lkplayer.log"));

    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &message) {
        static QMutex mutex;
        QMutexLocker locker(&mutex);

        const char *level = "info";
        switch (type) {
        case QtDebugMsg:
            level = "debug";
            break;
        case QtInfoMsg:
            level = "info";
            break;
        case QtWarningMsg:
            level = "warn";
            break;
        case QtCriticalMsg:
            level = "critical";
            break;
        case QtFatalMsg:
            level = "fatal";
            break;
        }

        QFile file(QDir(AppPaths::logDir()).absoluteFilePath(QStringLiteral("lkplayer.log")));
        if (!file.open(QIODevice::Append | QIODevice::Text))
            return;

        QTextStream stream(&file);
        stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " [" << level << "] "
               << (context.category ? context.category : "default") << ": " << message << '\n';

        if (type == QtFatalMsg)
            abort();
    });

    qCInfo(QLoggingCategory("lk.app")) << "logging to" << path;
}

/**
 * Prefer the modern Windows 11 look when the style is present and silently
 * fall back otherwise. The application hand rolls its own controls, so a
 * missing style is never fatal.
 */
void applyQuickStyle()
{
    QQuickStyle::setStyle(QStringLiteral("FluentWinUI3"));
    if (QQuickStyle::name().compare(QStringLiteral("FluentWinUI3"), Qt::CaseInsensitive) != 0)
        QQuickStyle::setStyle(QStringLiteral("Fusion"));
    qCInfo(QLoggingCategory("lk.app")) << "quick controls style:" << QQuickStyle::name();
}

/**
 * QQuickWindow seeds its font from QGuiApplication::font() and propagates it to
 * every child item, so that font *is* the system UI font the QML layer uses.
 *
 * On some locales Qt still resolves it to the legacy MS Shell Dlg 2 mapping
 * (宋体 / SimSun) or another bitmap face, so swap in a modern system typeface
 * when that happens. The Latin face stays first: DirectWrite then supplies the
 * matching CJK glyphs per character.
 */
void installSystemFont()
{
    QFont font = QGuiApplication::font();
    const QString resolved = QFontInfo(font).family();

    static const QStringList legacyFaces = {
        QStringLiteral("MS Sans Serif"), QStringLiteral("MS Shell Dlg"),
        QStringLiteral("MS Shell Dlg 2"), QStringLiteral("SimSun"),
        QStringLiteral("NSimSun"), QStringLiteral("Tahoma"),
    };

    if (!legacyFaces.contains(resolved, Qt::CaseInsensitive)) {
        qCInfo(QLoggingCategory("lk.app")) << "application font:" << resolved;
        return;
    }

    const QStringList available = QFontDatabase::families();
    static const QStringList preferred = {
        QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft JhengHei UI"),
        QStringLiteral("Segoe UI"), QStringLiteral("Yu Gothic UI"),
        QStringLiteral("Malgun Gothic"), QStringLiteral("Noto Sans CJK SC"),
    };
    for (const QString &candidate : preferred) {
        if (!available.contains(candidate, Qt::CaseInsensitive))
            continue;
        font.setFamily(candidate);
        QGuiApplication::setFont(font);
        qCInfo(QLoggingCategory("lk.app")) << "replaced legacy font" << resolved << "with" << QFontInfo(font).family();
        return;
    }
    qCWarning(QLoggingCategory("lk.app")) << "no modern system font available; keeping" << resolved;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("LkPlayer"));
    QCoreApplication::setApplicationName(QStringLiteral("LkPlayer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    installLogHandler();
    applyQuickStyle();
    installSystemFont();

    app.setWindowIcon(buildAppIcon());

    AppController controller;

    QQmlApplicationEngine engine;
    qmlRegisterSingletonInstance("LkPlayer", 1, 0, "App", &controller);
    engine.addImageProvider(QStringLiteral("thumb"), new ThumbnailProvider(controller.thumbnails()));

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("LkPlayer", "Main");

    return app.exec();
}
