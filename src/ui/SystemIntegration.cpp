#include "ui/SystemIntegration.h"
#include "core/Database.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#ifdef Q_OS_WIN
// SHQueryUserNotificationState y MOD_NOREPEAT necesitan Windows 7+ en los headers.
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0601
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace sysint {

namespace {
const char* kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char* kRunName = "UltimateTracker";
constexpr int kHotkeyId = 0x5554;   // "UT"
} // namespace

QString dailyBackup(Database& db, const QString& dataDir, const QString& dir, int keep)
{
    if (dir.isEmpty() && dataDir.isEmpty()) return {};
    const QString target = dir.isEmpty() ? QDir(dataDir).filePath(QStringLiteral("backups")) : dir;
    if (!QDir().mkpath(target)) return {};
    QDir d(target);
    const QString name = QStringLiteral("ultimatetracker-%1.db").arg(QDate::currentDate().toString(Qt::ISODate));
    QString made;
    if (!d.exists(name) && db.backupTo(d.filePath(name))) made = d.filePath(name);
    // Se quedan las más nuevas (el nombre lleva la fecha: ordenar por nombre alcanza).
    QStringList files;
    for (const QString& f : d.entryList({QStringLiteral("ultimatetracker-*.db")}, QDir::Files, QDir::Name))
        if (QDate::fromString(f.mid(16, 10), Qt::ISODate).isValid()) files << f;
    while (files.size() > keep) d.remove(files.takeFirst());
    return made;
}

void setAutostart(bool on)
{
#ifdef Q_OS_WIN
    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    if (on) {
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        run.setValue(QString::fromLatin1(kRunName), QStringLiteral("\"%1\" --autostart").arg(exe));
    } else {
        run.remove(QString::fromLatin1(kRunName));
    }
#else
    Q_UNUSED(on);
#endif
}

bool autostartEnabled()
{
#ifdef Q_OS_WIN
    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    return run.contains(QString::fromLatin1(kRunName));
#else
    return false;
#endif
}

bool userBusy()
{
#ifdef Q_OS_WIN
    QUERY_USER_NOTIFICATION_STATE state;
    if (SHQueryUserNotificationState(&state) != S_OK) return false;
    return state == QUNS_BUSY || state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE;
#else
    return false;
#endif
}

} // namespace sysint

// ---------------------------------------------------------------------------
GlobalHotkey::GlobalHotkey(QObject* parent)
    : QObject(parent)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

GlobalHotkey::~GlobalHotkey()
{
    setEnabled(false);
    if (QCoreApplication::instance()) QCoreApplication::instance()->removeNativeEventFilter(this);
}

bool GlobalHotkey::setEnabled(bool on)
{
#ifdef Q_OS_WIN
    if (on == m_registered) return true;
    if (on) {
        m_registered = RegisterHotKey(nullptr, sysint::kHotkeyId, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'P');
        return m_registered;
    }
    UnregisterHotKey(nullptr, sysint::kHotkeyId);
    m_registered = false;
    return true;
#else
    Q_UNUSED(on);
    return false;
#endif
}

bool GlobalHotkey::nativeEventFilter(const QByteArray& type, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(result);
    // Con hwnd nulo el mensaje llega a la cola del hilo ("dispatcher").
    if (type == "windows_generic_MSG" || type == "windows_dispatcher_MSG") {
        const MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == sysint::kHotkeyId) {
            emit activated();
            return true;
        }
    }
#else
    Q_UNUSED(type);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}
