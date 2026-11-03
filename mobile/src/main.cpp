#include "MobileApp.h"
#include "core/Database.h"
#include "core/DayClock.h"

#include <QDir>
#include <QFile>
#include <QUrl>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("UltimateTracker"));
    QGuiApplication::setOrganizationName(QStringLiteral("UltimateTracker"));

    QString dataDir = qEnvironmentVariable("ULTIMATETRACKER_DATA_DIR");
    if (dataDir.isEmpty()) dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    Database db;
    if (!db.open(QDir(dataDir).filePath(QStringLiteral("ultimatetracker.db")))) return 1;
    dayclock::setStartHour(db.setting("day_start_hour", 4).toInt());

    MobileApp ut(&db);
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &ut, [&ut](Qt::ApplicationState s) {
        if (s == Qt::ApplicationActive) ut.appActivated();
    });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ut"), &ut);
    // La ruta del recurso cambió en Qt 6.5 (/qt/qml/...): se prueba la nueva y la vieja.
    const QString path = QFile::exists(QStringLiteral(":/qt/qml/UltimateTracker/qml/Main.qml"))
        ? QStringLiteral("qrc:/qt/qml/UltimateTracker/qml/Main.qml") : QStringLiteral("qrc:/UltimateTracker/qml/Main.qml");
    engine.load(QUrl(path));
    if (engine.rootObjects().isEmpty()) return 1;
    // Para revisar la interfaz sin un celu: UT_SHOT=archivo.png saca una captura y sale.
    const QString shot = qEnvironmentVariable("UT_SHOT");
    if (!shot.isEmpty()) {
        QTimer::singleShot(qEnvironmentVariableIntValue("UT_SHOT_MS") > 0 ? qEnvironmentVariableIntValue("UT_SHOT_MS") : 1200, &app, [&engine, shot] {
            if (auto* w = qobject_cast<QQuickWindow*>(engine.rootObjects().first())) w->grabWindow().save(shot);
            QCoreApplication::quit();
        });
    }
    return app.exec();
}
