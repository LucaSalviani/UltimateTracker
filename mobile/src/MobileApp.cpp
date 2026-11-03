#include "MobileApp.h"
#include "AndroidBridge.h"
#include "core/DayClock.h"
#include "core/Format.h"

#include <QColor>
#include <QDateTime>
#include <QTimer>
#include <QVariantMap>
#include <functional>

namespace {
// Los mismos colores automáticos que la PC (ActivitiesWidget::autoColors).
const char* kAuto[] = {"#6f9bd1", "#d18f6f", "#8fc28a", "#c79bd6", "#d6c06f", "#6fc2bd", "#d16f8f", "#a8a8a8"};
}

MobileApp::MobileApp(Database* db, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_session(db)
    , m_sync(db)
{
    m_android = new AndroidBridge;
    m_session.setWorkSec(m_db->setting("work_min", 25).toInt() * 60);
    m_session.setBreakSec(m_db->setting("break_min", 10).toInt() * 60);
    reloadFolders();
    qint64 last = m_db->setting("last_folder").toLongLong();
    if (!m_tree.contains(last) && !m_tree.all().isEmpty()) last = m_tree.all().first().id;
    m_session.setFolderId(last);
    m_session.recoverAfterRestart();

    connect(&m_session, &SessionController::ticked, this, &MobileApp::ticked);
    connect(&m_session, &SessionController::stateChanged, this, [this] {
        emit changed();
        emit ticked();
        reloadToday();
        syncAndroid();
    });
    connect(&m_session, &SessionController::pomodoroSaved, this, [this] { reloadToday(); });
    connect(&m_session, &SessionController::pomodoroStarted, this, [this] { reloadToday(); });
    connect(&m_session, &SessionController::pomodoroCompleted, this, [this] {
        m_android->vibrate(400);
        emit completed();
    });
    connect(&m_session, &SessionController::breakFinished, this, [this] {
        m_android->vibrate(700);
        emit breakOver();
    });
    reloadToday();
    m_android->requestPermissions();

    // Sincronización: cada 30 s con la app abierta, al volver a ella y al terminar un pomodoro.
    connect(&m_sync, &SyncClient::changed, this, &MobileApp::syncChanged);
    connect(&m_sync, &SyncClient::message, this, &MobileApp::syncMessage);
    connect(&m_sync, &SyncClient::applied, this, [this] {
        reloadFolders();
        if (!m_tree.contains(m_session.folderId()) && !m_tree.all().isEmpty()) m_session.setFolderId(m_tree.all().first().id);
        emit changed();
        reloadToday();
    });
    connect(&m_session, &SessionController::pomodoroSaved, this, [this] { QTimer::singleShot(1500, &m_sync, &SyncClient::syncNow); });
    m_sync.setAutoSync(true, 30000);
}

MobileApp::~MobileApp()
{
    m_session.closeDay(false);
    m_android->stopRunning();
    delete m_android;
}

QString MobileApp::state() const
{
    using S = SessionController::State;
    switch (m_session.state()) {
    case S::Idle:      return QStringLiteral("idle");
    case S::Ready:     return QStringLiteral("ready");
    case S::Running:   return QStringLiteral("running");
    case S::Paused:    return QStringLiteral("paused");
    case S::WorkDone:  return QStringLiteral("workdone");
    case S::Break:     return QStringLiteral("break");
    case S::BreakOver: return QStringLiteral("breakover");
    }
    return {};
}

int MobileApp::remainingSec() const
{
    using S = SessionController::State;
    const S s = m_session.state();
    if (s == S::Idle || s == S::Ready || s == S::WorkDone) return m_session.workSec();
    return m_session.remainingSec();
}

double MobileApp::progress() const
{
    using S = SessionController::State;
    const S s = m_session.state();
    if (s == S::Running || s == S::Paused) return m_session.progress();
    if (s == S::Break) return m_session.breakSec() > 0 ? 1.0 - double(m_session.remainingSec()) / m_session.breakSec() : 0;
    return 0;
}

void MobileApp::setWorkMin(int m)
{
    m = qBound(1, m, 240);
    m_session.setWorkSec(m * 60);
    m_db->setSetting("work_min", m);
    emit changed();
    emit ticked();
}

void MobileApp::setBreakMin(int m)
{
    m = qBound(1, m, 120);
    m_session.setBreakSec(m * 60);
    m_db->setSetting("break_min", m);
    emit changed();
}

void MobileApp::nudgeWork(int d)
{
    const int m = workMin();
    // De a 5 arriba de 10, de a 1 por debajo (como en la PC).
    setWorkMin(d > 0 ? (m >= 10 ? (m / 5 + 1) * 5 : m + 1) : (m > 10 ? ((m - 1) / 5) * 5 : m - 1));
}

void MobileApp::nudgeBreak(int d)
{
    const int m = breakMin();
    setBreakMin(d > 0 ? (m >= 10 ? (m / 5 + 1) * 5 : m + 1) : (m > 10 ? ((m - 1) / 5) * 5 : m - 1));
}

QString MobileApp::folderName() const
{
    const Folder* f = m_tree.find(m_session.folderId());
    return f ? f->name : QStringLiteral("Sin actividad");
}

QString MobileApp::folderColor() const { return m_colors.value(m_session.folderId(), QStringLiteral("#a8a8a8")); }

QVariantList MobileApp::folders() const
{
    QVariantList out;
    std::function<void(qint64, int)> add = [&](qint64 parent, int depth) {
        for (qint64 id : m_tree.children(parent)) {
            QVariantMap m;
            m["id"] = id;
            m["name"] = m_tree.find(id)->name;
            m["depth"] = depth;
            m["color"] = m_colors.value(id);
            out.append(m);
            add(id, depth + 1);
        }
    };
    add(0, 0);
    return out;
}

void MobileApp::reloadFolders()
{
    m_tree = FolderTree(m_db->folders());
    const auto chosen = m_db->folderColors();
    m_colors.clear();
    int i = 0;
    for (const Folder& f : m_tree.all()) {
        const QString c = chosen.value(f.id);
        m_colors.insert(f.id, !c.isEmpty() ? c : QString::fromLatin1(kAuto[i % 8]));
        ++i;
    }
    emit foldersChanged();
}

void MobileApp::reloadToday()
{
    const QDate today = dayclock::today();
    const auto cur = m_session.currentPomodoro();
    m_today.clear();
    int n = 0;
    for (const Pomodoro& p : m_db->pomodorosBetween(today, today)) {
        if (p.status == PomoStatus::Parked) continue;
        const bool running = cur && cur->id == p.id;
        QVariantMap m;
        m["id"] = p.id;
        m["number"] = ++n;
        m["minutes"] = qMax(1, (running ? cur->plannedSec : p.representedSec()) / 60);
        m["running"] = running;
        m["color"] = m_colors.value(p.folderId, QStringLiteral("#a8a8a8"));
        m_today.append(m);
    }
    emit todayChanged();
    emit ticked();
}

int MobileApp::todayCount() const
{
    int n = 0;
    for (const QVariant& v : m_today) if (!v.toMap().value("running").toBool()) ++n;
    return n;
}

QString MobileApp::todayStudied() const
{
    const QDate today = dayclock::today();
    qint64 s = 0;
    const auto cur = m_session.currentPomodoro();
    for (const Pomodoro& p : m_db->pomodorosBetween(today, today))
        s += (cur && cur->id == p.id) ? cur->studiedSec : p.studiedSec;
    return fmt::duration(s);
}

QString MobileApp::dateLabel() const { return fmt::longDate(dayclock::today()); }

int MobileApp::pomodoroNumber() const
{
    return todayCount() + 1;   // el que corre (o el próximo)
}

QString MobileApp::clock(int sec) const
{
    sec = qMax(0, sec);
    return QStringLiteral("%1:%2").arg(sec / 60, 2, 10, QChar('0')).arg(sec % 60, 2, 10, QChar('0'));
}

void MobileApp::primary()
{
    using S = SessionController::State;
    switch (m_session.state()) {
    case S::Idle:
    case S::Ready:
    case S::BreakOver: m_session.start(); break;
    case S::Running:   m_session.pause(); break;
    case S::Paused:    m_session.resume(); break;
    case S::WorkDone:  m_session.start(); break;   // otro pomodoro sin recreo (con el recreo pendiente como indebido)
    case S::Break:     m_session.endBreakEarly(); break;
    }
}

void MobileApp::finishPomodoro() { m_session.endPomodoro(); }
void MobileApp::startBreak() { m_session.startBreak(); }
void MobileApp::skipBreak() { m_session.skipBreak(); }
void MobileApp::endBreak()
{
    if (m_session.state() == SessionController::State::BreakOver) m_session.silenceAlarm();
    else m_session.endBreakEarly();
}

void MobileApp::chooseFolder(qint64 id)
{
    m_session.setFolderId(id);
    m_db->setSetting("last_folder", id);
    emit changed();
    reloadToday();
    syncAndroid();
}

qint64 MobileApp::addFolder(const QString& name, qint64 parentId)
{
    const QString n = name.trimmed();
    if (n.isEmpty()) return 0;
    const qint64 id = m_db->addFolder(parentId, n);
    reloadFolders();
    return id;
}

void MobileApp::appActivated()
{
    m_session.tick();   // si el sistema la tuvo dormida, se pone al día con el reloj
    reloadToday();
    syncAndroid();
    m_sync.syncNow();
}

// La notificación con la cuenta regresiva y la alarma exacta para cuando termine.
void MobileApp::syncAndroid()
{
    using S = SessionController::State;
    const S s = m_session.state();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (s == S::Running || s == S::Break) {
        const qint64 end = now + qint64(m_session.remainingSec()) * 1000;
        const bool brk = s == S::Break;
        const QString title = brk ? QStringLiteral("Recreo") : QStringLiteral("Pomodoro · %1").arg(folderName());
        m_android->showRunning(title, brk ? QStringLiteral("Volvés en un rato") : QStringLiteral("A concentrarse"), end, brk);
        m_android->scheduleEnd(end, brk ? QStringLiteral("Terminó el recreo") : QStringLiteral("¡Pomodoro terminado!"),
                               brk ? QStringLiteral("A seguir estudiando") : QStringLiteral("Elegí si tomás un recreo"));
    } else if (s == S::Paused) {
        m_android->cancelEnd();
        m_android->showRunning(QStringLiteral("Pomodoro en pausa · %1").arg(folderName()),
                               QStringLiteral("Quedan %1").arg(clock(m_session.remainingSec())), 0, false);
    } else {
        m_android->cancelEnd();
        m_android->stopRunning();
    }
}
