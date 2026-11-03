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
#include <wtsapi32.h>
#endif
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

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

QString wakeTaskName() { return QStringLiteral("UltimateTracker - saludo"); }

#ifdef Q_OS_WIN
namespace {
int runSchtasks(const QStringList& args)
{
    QProcess p;
    p.start(QStringLiteral("schtasks.exe"), args);   // sin consola: QProcess usa CREATE_NO_WINDOW
    if (!p.waitForFinished(8000)) { p.kill(); return -1; }
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
}

QString taskXml(const QString& exe, const QString& user, bool withResume)
{
    auto esc = [](QString t) { return t.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;").replace('"', "&quot;"); };
    const QString resume = withResume ? QStringLiteral(
        "    <EventTrigger>\n"
        "      <Enabled>true</Enabled>\n"
        "      <Delay>PT10S</Delay>\n"
        "      <Subscription>&lt;QueryList&gt;&lt;Query Id=\"0\" Path=\"System\"&gt;&lt;Select Path=\"System\"&gt;"
        "*[System[Provider[@Name='Microsoft-Windows-Power-Troubleshooter'] and EventID=1]]"
        "&lt;/Select&gt;&lt;/Query&gt;&lt;/QueryList&gt;</Subscription>\n"
        "    </EventTrigger>\n") : QString();
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n"
        "<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
        "  <RegistrationInfo>\n"
        "    <Description>Abre UltimateTracker la primera vez que usás la PC en el día (al desbloquear o despertarla). "
        "Se apaga en Ajustes › Sistema.</Description>\n"
        "  </RegistrationInfo>\n"
        "  <Triggers>\n"
        "    <SessionStateChangeTrigger>\n"
        "      <Enabled>true</Enabled>\n"
        "      <Delay>PT5S</Delay>\n"
        "      <StateChange>SessionUnlock</StateChange>\n"
        "      <UserId>%2</UserId>\n"
        "    </SessionStateChangeTrigger>\n"
        "%3"
        "  </Triggers>\n"
        "  <Principals>\n"
        "    <Principal id=\"Author\">\n"
        "      <UserId>%2</UserId>\n"
        "      <LogonType>InteractiveToken</LogonType>\n"
        "      <RunLevel>LeastPrivilege</RunLevel>\n"
        "    </Principal>\n"
        "  </Principals>\n"
        "  <Settings>\n"
        "    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\n"
        "    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\n"
        "    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\n"
        "    <StartWhenAvailable>false</StartWhenAvailable>\n"
        "    <AllowStartOnDemand>true</AllowStartOnDemand>\n"
        "    <Enabled>true</Enabled>\n"
        "    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\n"
        "    <Priority>7</Priority>\n"
        "  </Settings>\n"
        "  <Actions Context=\"Author\">\n"
        "    <Exec>\n"
        "      <Command>%1</Command>\n"
        "      <Arguments>--wake</Arguments>\n"
        "    </Exec>\n"
        "  </Actions>\n"
        "</Task>\n").arg(esc(exe), esc(user), resume);
}
} // namespace
#endif

bool setWakeTask(bool on)
{
#ifdef Q_OS_WIN
    if (!on) {
        runSchtasks({QStringLiteral("/Delete"), QStringLiteral("/TN"), wakeTaskName(), QStringLiteral("/F")});
        return true;   // si no existía, también está bien
    }
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString user = qEnvironmentVariable("USERDOMAIN") + QLatin1Char('\\') + qEnvironmentVariable("USERNAME");
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("ultimatetracker-tarea.xml"));
    // Primero con los dos disparadores; si Windows no deja el de "despertar", solo con el de desbloquear.
    for (bool withResume : {true, false}) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        const QString xml = taskXml(exe, user, withResume);
        f.write("\xFF\xFE", 2);   // UTF-16 LE con BOM, como lo espera schtasks
        f.write(reinterpret_cast<const char*>(xml.utf16()), xml.size() * 2);
        f.close();
        const int rc = runSchtasks({QStringLiteral("/Create"), QStringLiteral("/TN"), wakeTaskName(),
                                    QStringLiteral("/XML"), path, QStringLiteral("/F")});
        QFile::remove(path);
        if (rc == 0) return true;
    }
    return false;
#else
    Q_UNUSED(on);
    return false;
#endif
}

} // namespace sysint

// ---------------------------------------------------------------------------
WakeWatcher::WakeWatcher(QObject* parent)
    : QObject(parent)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

WakeWatcher::~WakeWatcher()
{
#ifdef Q_OS_WIN
    if (m_hwnd) WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(m_hwnd));
#endif
    if (QCoreApplication::instance()) QCoreApplication::instance()->removeNativeEventFilter(this);
}

void WakeWatcher::attach(quintptr windowId)
{
#ifdef Q_OS_WIN
    if (m_hwnd == windowId) return;
    if (m_hwnd) WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(m_hwnd));
    m_hwnd = windowId;
    if (m_hwnd) WTSRegisterSessionNotification(reinterpret_cast<HWND>(m_hwnd), NOTIFY_FOR_THIS_SESSION);
#else
    m_hwnd = windowId;
#endif
}

bool WakeWatcher::nativeEventFilter(const QByteArray& type, void* message, qintptr* result)
{
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    if (type == "windows_generic_MSG") {
        const MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_WTSSESSION_CHANGE && msg->wParam == WTS_SESSION_UNLOCK) emit woke();
        if (msg->message == WM_POWERBROADCAST && (msg->wParam == PBT_APMRESUMEAUTOMATIC || msg->wParam == PBT_APMRESUMESUSPEND))
            emit woke();
    }
#else
    Q_UNUSED(type);
    Q_UNUSED(message);
#endif
    return false;   // que la ventana también los vea
}

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
