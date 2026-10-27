#include "ui/DevMode.h"
#include "core/Database.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>

namespace devmode {

namespace {
bool   g_active = false;
double g_speed = 1.0;
qint64 g_lastReal = 0;
qint64 g_virtual = 0;
QString flagPath(const QString& dataDir) { return QDir(dataDir).filePath(QStringLiteral("devmode")); }
} // namespace

bool enabledFlag(const QString& dataDir) { return !dataDir.isEmpty() && QFile::exists(flagPath(dataDir)); }

void setEnabledFlag(const QString& dataDir, bool on)
{
    if (dataDir.isEmpty()) return;
    if (on) {
        QFile f(flagPath(dataDir));
        if (f.open(QIODevice::WriteOnly)) f.write("Modo desarrollador: la app usa una copia de prueba de la base.\n");
    } else {
        QFile::remove(flagPath(dataDir));
    }
}

bool active() { return g_active; }
void setActive(bool on) { g_active = on; }

QString prepareSandbox(const QString& realDbPath, const QString& dataDir)
{
    const QString sandbox = QDir(dataDir).filePath(QStringLiteral("ultimatetracker-pruebas.db"));
    for (const QString& suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")})
        QFile::remove(sandbox + suffix);
    // Copia consistente de la real (incluye lo que esté en el WAL).
    Database real;
    if (!real.open(realDbPath) || !real.backupTo(sandbox)) return {};
    return sandbox;
}

double speed() { return g_speed; }

void setSpeed(double s)
{
    // Se congela el tiempo virtual actual para que el cambio de velocidad no salte.
    clock()();
    g_speed = qBound(1.0, s, 600.0);
}

std::function<qint64()> clock()
{
    return [] {
        const qint64 real = QDateTime::currentMSecsSinceEpoch();
        if (g_lastReal == 0) { g_lastReal = real; g_virtual = real; }
        g_virtual += qint64((real - g_lastReal) * g_speed);
        g_lastReal = real;
        return g_virtual;
    };
}

void jump(qint64 ms)
{
    clock()();
    g_virtual += ms;
}

void restartApp()
{
    // La nueva instancia espera a que esta termine (si no, el "una sola
    // instancia" la cerraría al ver esta todavía abierta).
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--restarted")});
}

} // namespace devmode
