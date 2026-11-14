#include "ui/FocusGuard.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
struct Front { QString title; QString exe; bool ours = false; };

Front frontWindow()
{
    Front f;
#ifdef Q_OS_WIN
    const HWND h = GetForegroundWindow();
    if (!h) return f;
    wchar_t buf[512];
    const int n = GetWindowTextW(h, buf, 512);
    f.title = QString::fromWCharArray(buf, qMax(0, n));
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    f.ours = pid == GetCurrentProcessId();
    if (HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t path[MAX_PATH];
        DWORD len = MAX_PATH;
        if (QueryFullProcessImageNameW(p, 0, path, &len)) f.exe = QFileInfo(QString::fromWCharArray(path, int(len))).completeBaseName();
        CloseHandle(p);
    }
#endif
    return f;
}
} // namespace

FocusGuard::FocusGuard(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(2000);
    connect(&m_timer, &QTimer::timeout, this, &FocusGuard::poll);
}

QString FocusGuard::match(const QString& title, const QString& exe, const QStringList& words)
{
    for (const QString& w : words) {
        const QString k = w.trimmed();
        if (k.size() < 3) continue;
        if (title.contains(k, Qt::CaseInsensitive) || exe.contains(k, Qt::CaseInsensitive)) return k;
    }
    return {};
}

void FocusGuard::configure(bool on, int graceSec, const QStringList& words)
{
    m_on = on;
    m_graceSec = qBound(0, graceSec, 600);
    m_words = words;
    setWatching(m_watching);
}

void FocusGuard::setWatching(bool on)
{
    m_watching = on;
    if (m_on && on) { if (!m_timer.isActive()) { m_awaySec = 0; m_fired = false; m_timer.start(); } }
    else m_timer.stop();
}

void FocusGuard::snooze(int seconds)
{
    m_snoozeUntil = QDateTime::currentSecsSinceEpoch() + seconds;
    m_awaySec = 0;
    m_fired = false;
}

void FocusGuard::poll()
{
    if (QDateTime::currentSecsSinceEpoch() < m_snoozeUntil) return;
    const Front f = frontWindow();
    const QString hit = f.ours ? QString() : match(f.title, f.exe, m_words);
    if (hit.isEmpty()) {
        m_awaySec = 0;
        m_fired = false;
        return;
    }
    m_awaySec += m_timer.interval() / 1000;
    if (!m_fired && m_awaySec >= m_graceSec) {
        m_fired = true;
        emit distracted(hit);
    }
}

// ---------------------------------------------------------------------------
FocusAlarm::FocusAlarm(QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    auto* title = new QLabel(QStringLiteral("RECREO INDEBIDO"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("color: white; font-size: 54pt; font-weight: 800; letter-spacing: 4px; background: transparent;"));
    m_what = new QLabel(this);
    m_what->setAlignment(Qt::AlignCenter);
    m_what->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.92); font-size: 16pt; background: transparent;"));
    m_clock = new QLabel(this);
    m_clock->setAlignment(Qt::AlignCenter);
    m_clock->setStyleSheet(QStringLiteral("color: white; font-size: 30pt; font-weight: 600; background: transparent;"));
    auto* back = new QPushButton(QStringLiteral("Volver al pomodoro"), this);
    back->setCursor(Qt::PointingHandCursor);
    back->setStyleSheet(QStringLiteral("QPushButton { background: white; color: #b3261e; border: none; border-radius: 12px; padding: 14px 28px; font-size: 14pt; font-weight: 700; }"
                                       "QPushButton:hover { background: #f3f3f3; }"));
    auto* ok = new QPushButton(QStringLiteral("Era necesario (no avisar por 10 min)"), this);
    ok->setCursor(Qt::PointingHandCursor);
    ok->setStyleSheet(QStringLiteral("QPushButton { background: transparent; color: rgba(255,255,255,0.85); border: 1px solid rgba(255,255,255,0.5); border-radius: 12px; padding: 12px 20px; font-size: 11pt; }"
                                     "QPushButton:hover { background: rgba(255,255,255,0.12); }"));
    connect(back, &QPushButton::clicked, this, [this] { hide(); emit backToWork(); });
    connect(ok, &QPushButton::clicked, this, [this] { hide(); emit needed(); });

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(16);
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(back);
    buttons->addStretch();
    auto* lay = new QVBoxLayout(this);
    lay->addStretch(3);
    lay->addWidget(title);
    lay->addSpacing(8);
    lay->addWidget(m_what);
    lay->addSpacing(18);
    lay->addWidget(m_clock);
    lay->addSpacing(28);
    lay->addLayout(buttons);
    lay->addStretch(4);

    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        const qint64 s = QDateTime::currentSecsSinceEpoch() - m_since;
        m_clock->setText(QStringLiteral("%1:%2").arg(s / 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0')));
        if (!isVisible()) m_tick.stop();
    });
}

void FocusAlarm::showFor(const QString& what, bool paused)
{
    m_what->setText(paused ? QStringLiteral("Estabas en %1. El pomodoro quedó en pausa.").arg(what)
                           : QStringLiteral("Estabas en %1 y el pomodoro sigue corriendo.").arg(what));
    m_since = QDateTime::currentSecsSinceEpoch();
    m_clock->setText(QStringLiteral("00:00"));
    // En la pantalla donde está el mouse (donde estabas mirando).
    QScreen* scr = QGuiApplication::screenAt(QCursor::pos());
    if (!scr) scr = QGuiApplication::primaryScreen();
    setGeometry(scr->geometry());
    show();
    raise();
    activateWindow();
    m_tick.start();
}

void FocusAlarm::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(179, 38, 30, 242));
}

void FocusAlarm::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) { hide(); emit backToWork(); return; }
    QWidget::keyPressEvent(e);
}
