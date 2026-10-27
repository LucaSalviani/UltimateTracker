#include "core/Database.h"
#include "core/DayClock.h"
#include "ui/MainWindow.h"
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
    QApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    // Vive en la bandeja: cerrar la ventana con algo en marcha no cierra la app.
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // Una sola instancia: si ya está abierta, la traemos al frente.
    const QString serverName = QStringLiteral("ultimatetracker-") + qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER"));
    {
        QLocalSocket probe;
        probe.connectToServer(serverName);
        if (probe.waitForConnected(300)) {
            probe.write("show");
            probe.waitForBytesWritten(300);
            return 0;
        }
    }
    QLocalServer::removeServer(serverName);
    QLocalServer server;
    server.listen(serverName);

    // Datos en %APPDATA%\UltimateTracker (o ULTIMATETRACKER_DATA_DIR para pruebas).
    QString dataDir = qEnvironmentVariable("ULTIMATETRACKER_DATA_DIR");
    if (dataDir.isEmpty()) dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString dbPath = QDir(dataDir).filePath(QStringLiteral("ultimatetracker.db"));
    migrateOldData(dataDir, dbPath);
    app.setProperty("dataDir", dataDir);   // para las copias de seguridad (M9)

    Database db;
    if (!db.open(dbPath)) {
        QMessageBox::critical(nullptr, QStringLiteral("UltimateTracker"),
                              QStringLiteral("No pude abrir la base de datos:\n%1").arg(db.lastError()));
        return 1;
    }
    dayclock::setStartHour(db.setting("day_start_hour", 4).toInt());

    // Tema de colores (paleta + hoja de estilos)
    theme::apply(db.setting("theme", "dark").toString());

    MainWindow w(&db);
    QObject::connect(&server, &QLocalServer::newConnection, &w, [&] {
        while (QLocalSocket* s = server.nextPendingConnection()) {
            s->deleteLater();
            w.bringToFront();
        }
    });
    w.show();
    return app.exec();
}
