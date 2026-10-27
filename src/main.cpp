#include "core/Database.h"
#include "core/DayClock.h"
#include "ui/MainWindow.h"

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

// Paleta gris: Fusion + estos colores evitan los acentos azules y las
// selecciones redondeadas del estilo nativo de Windows 11.
QPalette darkPalette()
{
    QPalette p;
    const QColor bg(0x1c, 0x1c, 0x1c), base(0x22, 0x22, 0x22), text(0xe8, 0xe8, 0xe8);
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, bg);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(0x2a, 0x2a, 0x2a));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, QColor(0x2b, 0x2b, 0x2b));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Highlight, QColor(0x3a, 0x3a, 0x3a));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(0x70, 0x70, 0x70));
    p.setColor(QPalette::Link, text);
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x5a, 0x5a, 0x5a));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x5a, 0x5a, 0x5a));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x5a, 0x5a, 0x5a));
    return p;
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
    QApplication::setPalette(darkPalette());

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

    Database db;
    if (!db.open(dbPath)) {
        QMessageBox::critical(nullptr, QStringLiteral("UltimateTracker"),
                              QStringLiteral("No pude abrir la base de datos:\n%1").arg(db.lastError()));
        return 1;
    }
    dayclock::setStartHour(db.setting("day_start_hour", 4).toInt());

    QFile qss(QStringLiteral(":/style.qss"));
    if (qss.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(qss.readAll()));

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
