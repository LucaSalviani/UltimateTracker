#pragma once

#include "core/Database.h"

#include <QObject>
#include <QTimer>
#include <functional>
#include <optional>

// Motor de la sesión de estudio. Es una máquina de estados que lleva el tiempo
// de cada pomodoro, recreo y "recreo indebido", y lo va guardando en la base.
//
//   Closed ──start──▶ Running ◀──resume── Paused
//                       │  ▲                ▲
//          (se cumple)  │  └──pause─────────┘
//                       ▼
//                    WorkDone ──startBreak──▶ Break ──(se cumple)──▶ BreakOver
//                       │                       │                       │
//                   skipBreak             endBreakEarly            silenceAlarm
//                       └──────────▶ Ready ◀────┴───────────────────────┘
//                                     │
//                                   start ──▶ Running
//
// endSession() vuelve a Closed desde cualquier estado.
// Tiempo que cuenta como indebido: Ready, Paused, WorkDone y BreakOver.
class SessionController : public QObject {
    Q_OBJECT
public:
    enum class State { Closed, Ready, Running, Paused, WorkDone, Break, BreakOver };
    Q_ENUM(State)

    using Clock = std::function<qint64()>; // milisegundos epoch

    explicit SessionController(Database* db, Clock clock = {}, bool autoTick = true,
                               QObject* parent = nullptr);

    State state() const { return m_state; }
    bool  sessionOpen() const { return m_state != State::Closed; }

    // ---- Configuración del próximo pomodoro / recreo --------------------
    qint64 folderId() const { return m_folderId; }
    void   setFolderId(qint64 id);
    int    workSec() const { return m_workSec; }
    void   setWorkSec(int sec);
    int    breakSec() const { return m_breakSec; }
    void   setBreakSec(int sec);

    // ---- Acciones --------------------------------------------------------
    void start();          // Closed / Ready / WorkDone / BreakOver → Running
    void pause();          // Running → Paused
    void resume();         // Paused → Running
    void startBreak();     // WorkDone → Break
    void skipBreak();      // WorkDone → Ready
    void endBreakEarly();  // Break → Ready
    void silenceAlarm();   // BreakOver → Ready
    void endSession();     // * → Closed

    // Avanza el reloj. Lo llama un QTimer interno (o los tests a mano).
    void tick();

    // Al abrir la app: un pomodoro que quedó "corriendo" (cierre o corte de luz)
    // se guarda hasta donde llegó y se marca final.
    void recoverAfterRestart();

    // ---- Lectura para la interfaz --------------------------------------
    std::optional<Pomodoro> currentPomodoro() const;   // con el tiempo al día
    double progress() const;                           // 0..1 del pomodoro actual
    int    remainingSec() const;                       // pomodoro o recreo
    int    stateElapsedSec() const;                    // tiempo en el estado actual
    qint64 sessionStudySec() const { return m_sesStudyMs / 1000; }
    qint64 sessionBreakSec() const { return m_sesBreakMs / 1000; }
    qint64 sessionUndueSec() const { return m_sesUndueMs / 1000; }

signals:
    void stateChanged(SessionController::State state);
    void ticked();
    void pomodoroStarted(qint64 id);
    void pomodoroCompleted(qint64 id);          // terminó su tiempo (sonido de progreso)
    void pomodoroFinalized(qint64 id);          // cambió a estado final (rojo)
    void studyRecorded(qint64 folderId, qint64 seconds); // para hitos
    void breakFinished();                       // alarma
    void sessionEnded(qint64 studySec, qint64 breakSec, qint64 undueSec);

private:
    enum class IvKind { None, Break, Undue };
    static IvKind kindFor(State s);

    qint64 now() const { return m_clock(); }
    QDate  today() const;
    void   setState(State s);
    void   openInterval(IvKind kind);
    void   closeInterval();
    void   addToInterval(qint64 ms);
    void   finishPomodoro(PomoStatus status);
    void   persist();

    Database* m_db;
    Clock     m_clock;
    QTimer    m_timer;

    State  m_state  = State::Closed;
    qint64 m_lastMs = 0;
    qint64 m_stateMs = 0;
    qint64 m_lastPersistMs = 0;

    qint64 m_folderId = 0;
    int    m_workSec  = 25 * 60;
    int    m_breakSec = 10 * 60;

    bool     m_hasPomo = false;
    Pomodoro m_pomo;
    qint64   m_pomoMs = 0;
    qint64   m_lastPomoId = 0;

    qint64 m_breakMs = 0;
    qint64 m_breakPlannedMs = 0;

    IvKind m_ivKind = IvKind::None;
    qint64 m_ivId = 0;
    qint64 m_ivMs = 0;
    QDate  m_ivDay;

    qint64 m_sesStudyMs = 0;
    qint64 m_sesBreakMs = 0;
    qint64 m_sesUndueMs = 0;
};
