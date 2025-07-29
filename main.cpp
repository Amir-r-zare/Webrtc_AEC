#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "audiocontroller.h"
#include "WebrtcAEC3.h"



int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    // Register the AudioController type with QML
    qmlRegisterType<AudioController>("Audio", 1, 0, "AudioController");

    QQmlApplicationEngine engine;

    // Create and expose AudioController instance
    AudioController audioController;
    WebrtcAEC3 webrtcAec;

//    engine.rootContext()->setContextProperty("webrtcAec", &webrtcAec);

    engine.rootContext()->setContextProperty("audioController", &audioController);


    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
