#include "core/Database.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Constancia"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    // Vive en la bandeja: cerrar la ventana no cierra la app si hay sesión abierta.
    QApplication::setQuitOnLastWindowClosed(false);

    // Una sola instancia: si ya está abierta, la traemos al frente.
    const QString serverName = QStringLiteral("constancia-") + qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER"));
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

    // Datos en %APPDATA%\Constancia (o CONSTANCIA_DATA_DIR para pruebas).
    QString dataDir = qEnvironmentVariable("CONSTANCIA_DATA_DIR");
    if (dataDir.isEmpty()) dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    Database db;
    if (!db.open(QDir(dataDir).filePath(QStringLiteral("constancia.db")))) {
        QMessageBox::critical(nullptr, QStringLiteral("Constancia"),
                              QStringLiteral("No pude abrir la base de datos:\n%1").arg(db.lastError()));
        return 1;
    }

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
