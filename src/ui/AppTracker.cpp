#include "ui/AppTracker.h"

#include "core/AppUsage.h"
#include "core/Database.h"
#include "core/DayClock.h"

#include <QDateTime>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr int kPollMs = 5000;
constexpr int kFlushSec = 60;

struct Front { QString exe, title; bool valid = false; };

#ifdef Q_OS_WIN
qint64 idleSeconds()
{
    LASTINPUTINFO li{};
    li.cbSize = sizeof(li);
    if (!GetLastInputInfo(&li)) return 0;
    return qint64((GetTickCount() - li.dwTime) / 1000);   // GetTickCount es de 32 bits: la resta con wrap anda bien
}
#else
qint64 idleSeconds() { return 0; }
#endif

Front front()
{
    Front f;
#ifdef Q_OS_WIN
    const HWND h = GetForegroundWindow();
    if (!h) return f;
    wchar_t buf[512];
    const int n = GetWindowTextW(h, buf, 512);
    f.title = QString::fromWCharArray(buf, qMax(0, n)).trimmed();
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t path[MAX_PATH];
        DWORD len = MAX_PATH;
        if (QueryFullProcessImageNameW(p, 0, path, &len)) f.exe = QFileInfo(QString::fromWCharArray(path, int(len))).completeBaseName();
        CloseHandle(p);
    }
    f.valid = !f.exe.isEmpty();
#endif
    return f;
}
} // namespace

AppTracker::AppTracker(Database* db, QObject* parent)
    : QObject(parent), m_db(db)
{
    m_timer.setInterval(kPollMs);
    connect(&m_timer, &QTimer::timeout, this, &AppTracker::poll);
}

void AppTracker::setEnabled(bool on)
{
    if (on == m_on) return;
    m_on = on;
    if (on) { m_last = QDateTime::currentSecsSinceEpoch(); m_timer.start(); }
    else { m_timer.stop(); flush(); }
}

void AppTracker::poll()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 dt = now - m_last;
    m_last = now;
    if (dt <= 0) return;
    if (dt > 3 * kPollMs / 1000) dt = kPollMs / 1000;   // la PC durmió o la app se colgó: no regalar tiempo
    if (idleSeconds() >= appusage::kIdleSec) return;     // sin tocar nada: no cuenta
    const Front f = front();
    if (!f.valid) return;
    const Key k{f.exe, f.title, m_inPomo, dayclock::logicalDate(QDateTime::currentDateTime())};
    bool found = false;
    for (auto& p : m_pending) if (p.first == k) { p.second += dt; found = true; break; }
    if (!found) m_pending.append({k, dt});
    m_sinceFlush += int(dt);
    if (m_sinceFlush >= kFlushSec) flush();
}

void AppTracker::flush()
{
    for (const auto& p : m_pending) appusage::add(*m_db, p.first.day, p.first.exe, p.first.title, p.first.pomo, p.second);
    m_pending.clear();
    m_sinceFlush = 0;
}
