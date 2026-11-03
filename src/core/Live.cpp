#include "core/Live.h"
#include "core/Database.h"
#include "core/DayClock.h"
#include "core/Format.h"
#include "core/Sync.h"

#include <QDateTime>
#include <QUuid>

namespace {
using S = SessionController::State;

const char* stateName(S s)
{
    switch (s) {
    case S::Idle:      return "idle";
    case S::Ready:     return "ready";
    case S::Running:   return "running";
    case S::Paused:    return "paused";
    case S::WorkDone:  return "workdone";
    case S::Break:     return "break";
    case S::BreakOver: return "breakover";
    }
    return "idle";
}

S stateFrom(const QString& n)
{
    if (n == QLatin1String("ready")) return S::Ready;
    if (n == QLatin1String("running")) return S::Running;
    if (n == QLatin1String("paused")) return S::Paused;
    if (n == QLatin1String("workdone")) return S::WorkDone;
    if (n == QLatin1String("break")) return S::Break;
    if (n == QLatin1String("breakover")) return S::BreakOver;
    return S::Idle;
}

qint64 num(const QJsonObject& o, const char* k) { return qint64(o.value(QLatin1String(k)).toDouble()); }
constexpr qint64 kAnnounceWindowMs = 90 * 1000;   // si se cumplió hace más que esto, no suena
} // namespace

LiveLink::LiveLink(Database* db, SessionController* session, SyncClient* sync, const QString& label, Clock clock, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_session(session)
    , m_sync(sync)
    , m_label(label)
    , m_clock(clock ? std::move(clock) : Clock([] { return QDateTime::currentMSecsSinceEpoch(); }))
{
    m_device = m_db->setting("sync/device").toString();
    if (m_device.isEmpty()) {
        m_device = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_db->setSetting("sync/device", m_device);
    }
    m_v = m_db->setting("live/v", 0).toLongLong();

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &LiveLink::publish);
    connect(&m_poll, &QTimer::timeout, this, &LiveLink::pollNow);

    // Lo que toca este aparato se manda; lo que llega del otro no se reenvía.
    auto local = [this] { if (!m_applying && !m_session->passive() && !m_session->releasing()) schedulePublish(); };
    connect(m_session, &SessionController::stateChanged, this, local);
    connect(m_session, &SessionController::pomodoroChanged, this, local);
    connect(m_session, &SessionController::tookOver, this, [this] { schedulePublish(); emit changed(); });

    if (m_sync) {
        connect(m_sync, &SyncClient::liveReceived, this, &LiveLink::receive);
        connect(m_sync, &SyncClient::livePushed, this, [this](bool ok) { if (!ok) m_pending = true; });
        // Llegaron actividades o pomodoros nuevos: la foto del otro ahora se entiende entera.
        connect(m_sync, &SyncClient::applied, this, [this] {
            if (m_session->passive() && !m_last.isEmpty()) applyRemote(m_last, false);
        });
        connect(m_sync, &SyncClient::changed, this, [this] {
            if (!m_sync->signedIn()) { m_ready = false; if (m_session->passive()) { m_applying = true; m_session->releaseToIdle(); m_applying = false; emit changed(); } }
        });
    }
}

void LiveLink::setPollInterval(int ms)
{
    if (ms <= 0) { m_poll.stop(); return; }
    if (m_poll.isActive() && m_poll.interval() == ms) return;
    m_poll.start(ms);
}

void LiveLink::pollNow()
{
    if (!m_sync) return;
    m_sync->fetchLive();
    // Espejo de un pomodoro que todavía no llegó: se pide la sincronización.
    if (m_session->passive() && !m_last.value("pomo").toString().isEmpty() &&
        m_db->idForUid("pomodoros", m_last.value("pomo").toString()) <= 0)
        m_sync->syncNow();
}

bool LiveLink::mirroring() const { return m_session->passive(); }

QJsonObject LiveLink::snapshot() const
{
    const auto s = m_session->liveState();
    QJsonObject o{{"device", m_device}, {"label", m_label}, {"state", QString::fromLatin1(stateName(s.state))},
                  {"at", m_clock()}, {"folder", m_db->uidFor("folders", s.folderId)},
                  {"break_ms", s.breakMs}, {"break_planned_ms", s.breakPlannedMs}};
    if (!s.pomoUid.isEmpty()) {
        o.insert("pomo", s.pomoUid);
        o.insert("pomo_ms", s.pomoMs);
        o.insert("pomo_folder", m_db->uidFor("folders", s.pomo.folderId));
        o.insert("pomo_day", fmt::dayKey(s.pomo.day));
        o.insert("pomo_started", s.pomo.startedAt);
        o.insert("pomo_ended", s.pomo.endedAt);
        o.insert("pomo_planned", s.pomo.plannedSec);
        o.insert("pomo_linked", s.pomo.linked);
    }
    return o;
}

void LiveLink::schedulePublish() { m_debounce.start(); }

void LiveLink::flush()
{
    if (!m_debounce.isActive()) return;
    m_debounce.stop();
    publish();
}

void LiveLink::publish()
{
    if (m_session->passive()) return;
    if (m_sync && !m_sync->signedIn()) return;
    if (m_sync && !m_ready) { m_pending = true; pollNow(); return; }
    m_pending = false;
    ++m_v;
    m_owner = m_device;
    m_db->setSetting("live/v", m_v);
    const QJsonObject data = snapshot();
    emit published(data, m_v);
    if (m_sync) {
        m_sync->pushLive(data, m_v);
        m_sync->syncNow();   // que el pomodoro también llegue enseguida al otro aparato
    }
}

void LiveLink::receive(const QJsonObject& data)
{
    const bool first = !m_ready;
    m_ready = true;
    if (data.isEmpty()) {   // todavía no hay nada en el servidor
        if (m_pending) publish();
        return;
    }
    const qint64 v = num(data, "v");
    const QString dev = data.value("device").toString();
    if (dev == m_device) {
        // Es la mía. Si quedó "corriendo" de antes (la app se cerró) y acá ya no corre, se corrige.
        if (v > m_v) { m_v = v; m_db->setSetting("live/v", m_v); }
        m_owner = m_device;
        const S remote = stateFrom(data.value("state").toString());
        const auto here = m_session->state();
        if (first && remote != S::Idle && remote != S::Ready && (here == S::Idle || here == S::Ready)) m_pending = true;
        if (m_pending) publish();
        return;
    }
    const bool accept = v > m_v || (v == m_v && m_owner != dev);
    if (!accept) {
        if (m_pending) publish();
        return;
    }
    m_v = v;
    m_owner = dev;
    m_db->setSetting("live/v", m_v);
    m_pending = false;   // lo del otro es más nuevo: lo de acá que no salió se pierde
    applyRemote(data, true);
}

void LiveLink::applyRemote(const QJsonObject& data, bool announce)
{
    m_applying = true;
    const qint64 now = m_clock();
    const qint64 at = num(data, "at");
    S st = stateFrom(data.value("state").toString());
    // Una foto de otro día ya no vale (el día se cerró).
    if (dayclock::logicalDate(QDateTime::fromMSecsSinceEpoch(at)) != dayclock::logicalDate(QDateTime::fromMSecsSinceEpoch(now))) st = S::Idle;

    if (st == S::Idle || st == S::Ready) {
        if (m_session->passive() || m_session->state() != S::Idle) m_session->releaseToIdle();
        m_last = {};
        m_remoteLabel.clear();
        m_applying = false;
        emit changed();
        return;
    }

    SessionController::LiveState s;
    s.state = st;
    s.folderId = m_db->idForUid("folders", data.value("folder").toString());
    s.pomoUid = data.value("pomo").toString();
    if (!s.pomoUid.isEmpty()) {
        s.pomo.id = m_db->idForUid("pomodoros", s.pomoUid);
        s.pomo.folderId = m_db->idForUid("folders", data.value("pomo_folder").toString());
        s.pomo.day = fmt::fromDayKey(data.value("pomo_day").toString());
        s.pomo.startedAt = num(data, "pomo_started");
        s.pomo.endedAt = num(data, "pomo_ended");
        s.pomo.plannedSec = int(num(data, "pomo_planned"));
        s.pomo.linked = data.value("pomo_linked").toBool();
        s.pomo.status = PomoStatus::Running;
        s.pomoMs = num(data, "pomo_ms");
        if (s.pomo.id <= 0 && m_sync) m_sync->syncNow();   // todavía no llegó: se pide
    }
    s.breakMs = num(data, "break_ms");
    s.breakPlannedMs = num(data, "break_planned_ms");

    // Se pone al día con lo que pasó desde la foto.
    const qint64 gone = qMax<qint64>(0, now - at);
    bool loud = announce;
    if (st == S::Running) {
        const qint64 planned = qint64(s.pomo.plannedSec) * 1000;
        s.pomoMs += gone;
        if (s.pomoMs >= planned) {
            loud = loud && s.pomoMs - planned < kAnnounceWindowMs;
            s.pomoMs = planned;
            s.pomo.endedAt = (at + (planned - num(data, "pomo_ms"))) / 1000;
            s.state = S::WorkDone;
            // Para que suene como si hubiera terminado acá: primero corriendo, después hecho.
            if (loud && m_session->state() != S::Running) {
                SessionController::LiveState r = s;
                r.state = S::Running;
                r.pomoMs = planned - 1;
                m_session->adoptRemote(r, false);
            }
        }
    } else if (st == S::Break) {
        s.breakMs += gone;
        if (s.breakMs >= s.breakPlannedMs) {
            loud = loud && s.breakMs - s.breakPlannedMs < kAnnounceWindowMs;
            s.breakMs = s.breakPlannedMs;
            s.state = S::BreakOver;
            if (loud && m_session->state() != S::Break) {
                SessionController::LiveState r = s;
                r.state = S::Break;
                r.breakMs = s.breakPlannedMs - 1;
                m_session->adoptRemote(r, false);
            }
        }
    }
    m_session->adoptRemote(s, loud);
    m_last = data;
    const QString lbl = data.value("label").toString();
    m_remoteLabel = lbl.isEmpty() ? QStringLiteral("el otro aparato") : lbl;
    m_applying = false;
    emit changed();
}
