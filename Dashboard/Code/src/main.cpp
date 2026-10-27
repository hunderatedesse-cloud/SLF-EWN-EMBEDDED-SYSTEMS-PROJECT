#include "controller/AppController.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickStyle>
#include <QStandardPaths>

namespace {

class UserAgentNam : public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &original, QIODevice *outgoingData) override
    {
        QNetworkRequest req(original);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("SLF-EWN/1.0 (subsurface-early-warning; Qt)"));
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        return QNetworkAccessManager::createRequest(op, req, outgoingData);
    }
};

class SlfNamFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override
    {
        auto *nam = new UserAgentNam(parent);
        auto *cache = new QNetworkDiskCache(nam);
        cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                                 + QStringLiteral("/map-tiles"));
        cache->setMaximumCacheSize(96LL * 1024 * 1024);
        nam->setCache(cache);
        return nam;
    }
};

SlfNamFactory &namFactory()
{
    static SlfNamFactory factory;
    return factory;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SLF-EWN"));
    app.setApplicationName(QStringLiteral("SLF-EWN"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setApplicationDisplayName(QStringLiteral("SLF-EWN  ·  Subsurface Early Warning"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    const QStringList bundledFonts = {
        QStringLiteral(":/SlfEwn/fonts/Inter-Regular.ttf"),
        QStringLiteral(":/SlfEwn/fonts/Inter-Medium.ttf"),
        QStringLiteral(":/SlfEwn/fonts/Inter-SemiBold.ttf"),
        QStringLiteral(":/SlfEwn/fonts/JetBrainsMono-Regular.ttf"),
    };
    for (const QString &path : bundledFonts)
        QFontDatabase::addApplicationFont(path);

    Slf::AppController controller;

    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&namFactory());
    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("Auth"), controller.auth());

    const QUrl url(QStringLiteral("qrc:/SlfEwn/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
