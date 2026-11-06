#include "core/Database.h"
#include "core/DayClock.h"
#include "ui/DevMode.h"
#include "ui/Greeting.h"
#include "ui/MainWindow.h"
#include "ui/WheelGuard.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QPalette>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QLockFile>

namespace {
// La app antes se llamaba "Constancia": si existen sus datos y todavía no hay
// datos nuevos, se copian (la carpeta vieja queda intacta como respaldo).
void migrateOldData(const QString& newDir, const QString& newDb)
{
    if (QFile::exists(newDb)) return;
    QDir base(newDir);
    base.cdUp();
    const QString oldDb = base.filePath(QStringLiteral("Constancia/constancia.db"));
    if (!QFile::exists(oldDb)) return;
    QFile::copy(oldDb, newDb);
    // Modo WAL: lo último escrito puede estar todavía en el archivo -wal.
    if (QFile::exists(oldDb + QStringLiteral("-wal")))
        QFile::copy(oldDb + QStringLiteral("-wal"), newDb + QStringLiteral("-wal"));
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("UltimateTracker"));
    QApplication::setApplicationDisplayName(QStringLiteral("UltimateTracker"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0-beta.2"));
    // Vive en la bandeja: cerrar la ventana con algo en marcha no cierra la app.
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.installEventFilter(new WheelGuard(&app));   // Mejoras 2.0: la rueda no cambia valores sin click

    // Datos en %APPDATA%\UltimateTracker (o ULTIMATETRACKER_DATA_DIR para pruebas).
    QString dataDir = qEnvironmentVariable("ULTIMATETRACKER_DATA_DIR");
    if (dataDir.isEmpty()) dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    // M28: candado de instancia. En un reinicio pedido por la app (modo
    // desarrollador) se espera a que la anterior termine del todo antes de seguir.
    QLockFile instanceLock(QDir(dataDir).filePath(QStringLiteral("instance.lock")));
    instanceLock.setStaleLockTime(0);
    const bool restarted = QCoreApplication::arguments().contains(QStringLiteral("--restarted"));
    const bool gotLock = instanceLock.tryLock(restarted ? 15000 : 0);
    if (restarted && !gotLock) return 0;

    // Una sola instancia: si ya está abierta, la traemos al frente.
    const QString serverName = QStringLiteral("ultimatetracker-") + qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER"));
    if (!restarted) {
        QLocalSocket probe;
        probe.connectToServer(serverName);
        if (probe.waitForConnected(300)) {
            // B2-4: la tarea de "despertar" no trae la ventana al frente: solo le avisa.
            probe.write(QCoreApplication::arguments().contains(QStringLiteral("--wake")) ? "wake" : "show");
            probe.waitForBytesWritten(300);
            return 0;
        }
    }
    QLocalServer::removeServer(serverName);
    QLocalServer server;
    server.listen(serverName);
    app.setProperty("instanceServer", QVariant::fromValue<QObject*>(&server));

    const QString dbPath = QDir(dataDir).filePath(QStringLiteral("ultimatetracker.db"));
    migrateOldData(dataDir, dbPath);
    app.setProperty("dataDir", dataDir);   // para las copias de seguridad (M9)

    // M34: bucle de "reinicio por dentro": al entrar o salir del modo de pruebas la
    // ventana pide salir con kRestartCode, se cierra la base y se vuelve a abrir todo
    // en el mismo proceso (sin depender de lanzar otro .exe).
    int exitCode = 0;
    for (bool first = true;; first = false) {
        // M20: en modo desarrollador se trabaja sobre una copia de la base.
        devmode::setActive(false);
        QString openPath = dbPath;
        if (devmode::enabledFlag(dataDir)) {
            const QString sandbox = devmode::prepareSandbox(dbPath, dataDir);
            if (!sandbox.isEmpty()) {
                openPath = sandbox;
                devmode::setActive(true);
            }
        }

        Database db;
        if (!db.open(openPath)) {
            QMessageBox::critical(nullptr, QStringLiteral("UltimateTracker"),
                                  QStringLiteral("No pude abrir la base de datos:\n%1").arg(db.lastError()));
            return 1;
        }
        dayclock::setStartHour(db.setting("day_start_hour", 4).toInt());
        // B2-4: abierta por la tarea al desbloquear o despertar la PC: si hoy ya saludó
        // (o el saludo está apagado), no abre nada.
        if (first && QCoreApplication::arguments().contains(QStringLiteral("--wake")) && !Greeting::shouldShow(db)) return 0;

        // Tema de colores (paleta + hoja de estilos)
        theme::apply(SettingsDialog::themeKey(&db));
        SettingsDialog::loadTileStyle(&db);

        MainWindow w(&db);
        const QRect keep = app.property("restartGeometry").toRect();
        if (!first && keep.isValid()) w.setGeometry(keep);   // misma posición que antes del reinicio
        QObject::connect(&server, &QLocalServer::newConnection, &w, [&] {
            while (QLocalSocket* s = server.nextPendingConnection()) {
                s->waitForReadyRead(300);
                const QByteArray what = s->readAll();
                s->deleteLater();
                if (what == "wake") w.onWake();
                else w.bringToFront();
            }
        });
        // M10: al iniciar con Windows (o con --greeting), una vez por día, saludo primero.
        const QStringList args = QCoreApplication::arguments();
        const bool forceGreeting = args.contains(QStringLiteral("--greeting"));
        const bool wake = args.contains(QStringLiteral("--wake"));
        // Mejoras 2.0: el saludo es una tarjeta sobre la misma ventana.
        if (first && (forceGreeting || ((wake || args.contains(QStringLiteral("--autostart"))) && Greeting::shouldShow(db)))) {
            w.show();
            w.showDayCard(true, forceGreeting);
        } else {
            w.show();
        }
        exitCode = app.exec();
        if (exitCode != devmode::kRestartCode) break;
    }
    return exitCode;
}