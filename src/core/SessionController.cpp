#include "core/SessionController.h"

#include <QDateTime>

namespace {
constexpr qint64 kPersistEveryMs = 5000;
constexpr int    kTickMs = 250;
constexpr int    kMinStudySec = 60;   // menos de esto no cuenta como pomodoro
} // namespace

SessionController::SessionController(Database* db, Clock clock, bool autoTick, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_clock(clock ? std::move(clock) : Clock([] { return QDateTime::currentMSecsSinceEpoch(); }))
{
    m_lastMs = now();
    if (autoTick) {
        m_timer.setInterval(kTickMs);
        m_timer.setTimerType(Qt::CoarseTimer);
        connect(&m_timer, &QTimer::timeout, this, &SessionController::tick);
        m_timer.start();
    }
}

QDate SessionController::today() const
{
    return QDateTime::fromMSecsSinceEpoch(now()).date();
}

bool SessionController::inProgress() const
{
    return m_state == State::Running || m_state == State::Paused ||
           m_state == State::Break || m_state == State::BreakOver;
}

SessionController::IvKind SessionController::kindFor(State s)
{
    switch (s) {
    case State::Ready:
    case State::Paused:
    case State::WorkDone:
    case State::BreakOver: return IvKind::Pending;
    case State::Break:     return IvKind::Break;
    default:               return IvKind::None;
    }
}

// ---------------------------------------------------------------------------
// Configuración
// ---------------------------------------------------------------------------
void SessionController::setFolderId(qint64 id)
{
    m_folderId = id;
    // Cambio "en caliente": si hay un pomodoro en curso, pasa entero a la actividad nueva.
    if (m_hasPomo && m_pomo.folderId != id) {
        m_pomo.folderId = id;
        m_db->setPomodoroFolder(m_pomo.id, id);
        emit pomodoroChanged();
    }
}

void SessionController::setWorkSec(int sec) { m_workSec = qMax(60, sec); }
void SessionController::setBreakSec(int sec) { m_breakSec = qMax(60, sec); }

// ---------------------------------------------------------------------------
// Reloj
// ---------------------------------------------------------------------------
void SessionController::tick()
{
    const qint64 t = now();
    const qint64 delta = qMax<qint64>(0, t - m_lastMs);
    m_lastMs = t;
    if (m_state == State::Idle) return;

    m_stateMs += delta;

    switch (m_state) {
    case State::Running: {
        const qint64 plannedMs = qint64(m_pomo.plannedSec) * 1000;
        m_pomoMs += delta;
        if (m_pomoMs >= plannedMs) {
            const qint64 over = m_pomoMs - plannedMs;
            m_pomoMs = plannedMs;
            const qint64 id = m_pomo.id;
            finishPomodoro();
            setState(State::WorkDone);
            addToInterval(over);
            emit pomodoroCompleted(id);
        }
        break;
    }
    case State::Break: {
        m_breakMs += delta;
        if (m_breakMs >= m_breakPlannedMs) {
            const qint64 over = m_breakMs - m_breakPlannedMs;
            m_breakMs = m_breakPlannedMs;
            addToInterval(delta - over);
            setState(State::BreakOver);
            addToInterval(over);
            emit breakFinished();
        } else {
            addToInterval(delta);
        }
        break;
    }
    default:
        addToInterval(delta);
        break;
    }

    if (t - m_lastPersistMs >= kPersistEveryMs) persist();
    emit ticked();
}

// ---------------------------------------------------------------------------
// Acciones
// ---------------------------------------------------------------------------
void SessionController::start()
{
    tick();
    if (m_state == State::Running || m_state == State::Paused || m_state == State::Break) return;
    startPomodoro(false);
}

void SessionController::startPomodoro(bool linked)
{
    const qint64 t = now();
    m_pomo = Pomodoro{};
    m_pomo.folderId   = m_folderId;
    m_pomo.day        = today();
    m_pomo.startedAt  = t / 1000;
    m_pomo.endedAt    = t / 1000;
    m_pomo.plannedSec = m_workSec;
    m_pomo.status     = PomoStatus::Running;
    m_pomo.linked     = linked;
    m_pomo.id         = m_db->insertPomodoro(m_pomo);
    m_pomoMs  = 0;
    m_hasPomo = true;

    setState(State::Running);
    emit pomodoroStarted(m_pomo.id);
}

void SessionController::pause()
{
    tick();
    if (m_state != State::Running) return;
    persist();
    setState(State::Paused);
}

void SessionController::resume()
{
    tick();
    if (m_state != State::Paused) return;
    setState(State::Running);
}

void SessionController::endPomodoro()
{
    tick();
    if (m_state != State::Running && m_state != State::Paused) return;
    if (finishPomodoro()) setState(State::WorkDone);
    else                  setState(m_lastPomoId ? State::Ready : State::Idle);  // duró < 1 min
}

void SessionController::startBreak()
{
    tick();
    if (m_state != State::WorkDone) return;
    m_breakMs = 0;
    m_breakPlannedMs = qint64(m_breakSec) * 1000;
    setState(State::Break);
}

void SessionController::skipBreak()
{
    tick();
    if (m_state == State::WorkDone) startPomodoro(true);
}

void SessionController::endBreakEarly()
{
    tick();
    if (m_state == State::Break) setState(State::Ready);
}

void SessionController::silenceAlarm()
{
    tick();
    if (m_state == State::BreakOver) setState(State::Ready);
}

void SessionController::closeDay()
{
    tick();
    if (m_state == State::Idle) return;
    finishPomodoro();
    // Lo que quedó pendiente no se confirma: era el final del día (inactivo).
    setState(State::Idle);
    m_pendingIds.clear();
    m_lastPomoId = 0;
}

void SessionController::recoverAfterRestart()
{
    for (Pomodoro p : m_db->pomodorosWithStatus(PomoStatus::Running)) {
        if (p.studiedSec < kMinStudySec) {
            m_db->deletePomodoro(p.id);
        } else {
            p.status = PomoStatus::Completed;
            m_db->updatePomodoro(p);
        }
    }
    // Si cerraste la app hoy con tiempo muerto pendiente, sigue pendiente: si
    // volvés a estudiar hoy, cuenta como recreo indebido.
    const QDate d = today();
    for (const Interval& i : m_db->intervalsBetween(d, d))
        if (i.kind == IntervalKind::Pending) m_pendingIds.append(i.id);
}

// ---------------------------------------------------------------------------
// Internos
// ---------------------------------------------------------------------------
void SessionController::setState(State s)
{
    if (s == m_state) return;
    const IvKind newKind = kindFor(s);
    if (newKind != m_ivKind) {
        closeInterval();
        // Volviste a estudiar (o arrancaste un recreo): el tiempo muerto de
        // antes era un recreo indebido.
        if (s == State::Running || s == State::Break) confirmPending();
        if (newKind != IvKind::None) openInterval(newKind);
    }
    m_state = s;
    m_stateMs = 0;
    emit stateChanged(s);
}

void SessionController::openInterval(IvKind kind)
{
    m_ivKind = kind;
    m_ivMs = 0;
    m_ivDay = today();
    m_ivId = m_db->insertInterval(kind == IvKind::Break ? IntervalKind::Break : IntervalKind::Pending,
                                  m_ivDay, now() / 1000, 0);
    if (kind == IvKind::Pending) m_pendingIds.append(m_ivId);
}

void SessionController::closeInterval()
{
    if (m_ivKind == IvKind::None) return;
    m_db->updateIntervalSeconds(m_ivId, (m_ivMs + 500) / 1000);
    m_ivKind = IvKind::None;
    m_ivId = 0;
    m_ivMs = 0;
}

void SessionController::confirmPending()
{
    m_db->setIntervalsKind(m_pendingIds, IntervalKind::Undue);
    m_pendingIds.clear();
}

void SessionController::addToInterval(qint64 ms)
{
    if (m_ivKind == IvKind::None || ms <= 0) return;
    // Si pasó la medianoche, el resto va al día nuevo. Lo pendiente del día
    // anterior queda como inactivo para siempre.
    if (m_ivDay != today()) {
        const IvKind k = m_ivKind;
        closeInterval();
        m_pendingIds.clear();
        openInterval(k);
    }
    m_ivMs += ms;
}

bool SessionController::finishPomodoro()
{
    if (!m_hasPomo) return false;
    m_pomo.studiedSec = int((m_pomoMs + 500) / 1000);
    if (m_pomo.studiedSec < kMinStudySec) {
        // Menos de un minuto: no se registra.
        m_db->deletePomodoro(m_pomo.id);
        m_hasPomo = false;
        emit pomodoroSaved(0);
        return false;
    }
    m_pomo.endedAt    = now() / 1000;
    m_pomo.status     = PomoStatus::Completed;
    m_db->updatePomodoro(m_pomo);
    m_lastPomoId = m_pomo.id;
    m_hasPomo = false;
    emit studyRecorded(m_pomo.folderId, m_pomo.studiedSec);
    emit pomodoroSaved(m_pomo.id);
    return true;
}

void SessionController::persist()
{
    m_lastPersistMs = now();
    if (m_hasPomo) {
        m_pomo.studiedSec = int(m_pomoMs / 1000);
        m_pomo.endedAt = now() / 1000;
        m_db->updatePomodoro(m_pomo);
    }
    if (m_ivKind != IvKind::None)
        m_db->updateIntervalSeconds(m_ivId, m_ivMs / 1000);
}

// ---------------------------------------------------------------------------
// Lectura
// ---------------------------------------------------------------------------
std::optional<Pomodoro> SessionController::currentPomodoro() const
{
    if (!m_hasPomo) return std::nullopt;
    Pomodoro p = m_pomo;
    p.studiedSec = int(m_pomoMs / 1000);
    return p;
}

double SessionController::progress() const
{
    if (!m_hasPomo || m_pomo.plannedSec <= 0) return 0.0;
    return qBound(0.0, double(m_pomoMs) / (m_pomo.plannedSec * 1000.0), 1.0);
}

int SessionController::remainingSec() const
{
    switch (m_state) {
    case State::Running:
    case State::Paused:
        return int((qint64(m_pomo.plannedSec) * 1000 - m_pomoMs + 999) / 1000);
    case State::Break:
        return int((m_breakPlannedMs - m_breakMs + 999) / 1000);
    default:
        return 0;
    }
}

int SessionController::stateElapsedSec() const { return int(m_stateMs / 1000); }
