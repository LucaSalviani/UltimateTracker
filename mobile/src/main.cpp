#include "MobileApp.h"
#include "core/Database.h"
#include "core/DayClock.h"

#include <QDir>
#include <QFile>
#include <QUrl>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("UltimateTracker"));
    QGuiApplication::setOrganizationName(QStringLiteral("UltimateTracker"));
    QGuiApplication::setApplicationVersion(QStringLiteral("1.0.0-beta.2"));

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

    QQmlPropertyMap theme;   // antes que el motor: tiene que vivir más que él
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ut"), &ut);
    // Los colores y medidas de toda la app (los mismos grises que la PC).
    const std::pair<const char*, QVariant> values[] = {
        {"bg", "#161616"}, {"surface", "#222222"}, {"surfaceHi", "#2c2c2c"}, {"sheet", "#1e1e1e"},
        {"line", "#333333"}, {"text", "#ececec"}, {"dim", "#9a9a9a"}, {"faint", "#5f5f5f"},
        {"accent", "#e0483f"}, {"accentSoft", "#b85a54"}, {"brk", "#3cb89b"}, {"good", "#5fb878"},
        {"bronze", "#c08457"}, {"silver", "#b9c0c8"}, {"gold", "#e2b84a"}, {"margin", 20}, {"radius", 14}};
    for (const auto& [k, v] : values) theme.insert(QString::fromLatin1(k), v);
    engine.rootContext()->setContextProperty(QStringLiteral("Theme"), &theme);
    // La ruta del recurso cambió en Qt 6.5 (/qt/qml/...): se prueba la nueva y la vieja.
    const QString path = QFile::exists(QStringLiteral(":/qt/qml/UltimateTracker/qml/Main.qml"))
        ? QStringLiteral("qrc:/qt/qml/UltimateTracker/qml/Main.qml") : QStringLiteral("qrc:/UltimateTracker/qml/Main.qml");
    engine.load(QUrl(path));
    if (engine.rootObjects().isEmpty()) return 1;
    // Para revisar la interfaz sin un celu: UT_SHOT=archivo.png saca una captura y sale.
    if (QCoreApplication::arguments().contains(QStringLiteral("--demo-run"))) ut.primary();
    const QString shot = qEnvironmentVariable("UT_SHOT");
    if (!shot.isEmpty()) {
        QTimer::singleShot(qEnvironmentVariableIntValue("UT_SHOT_MS") > 0 ? qEnvironmentVariableIntValue("UT_SHOT_MS") : 1200, &app, [&engine, shot] {
            if (auto* w = qobject_cast<QQuickWindow*>(engine.rootObjects().first())) w->grabWindow().save(shot);
            QCoreApplication::exit(0);   // quit() lo frena «Atrás → Hoy»
        });
    }
    return app.exec();
}
