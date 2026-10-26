#pragma once

#include "core/Database.h"

#include <QObject>
#include <QTimer>
#include <functional>
#include <optional>

// Motor de la jornada de estudio. Es una máquina de estados que lleva el tiempo
// de cada pomodoro, recreo y "recreo indebido", y lo va guardando en la base.
//
//   Idle ──start──▶ Running ◀──resume── Paused
//                    │  ▲  │               ▲
//   (se cumple o     │  │  └──pause────────┘
//    endPomodoro)    ▼  │
//                  WorkDone ──startBreak──▶ Break ──(se cumple)──▶ BreakOver
//                    │                        │                       │
//                skipBreak               endBreakEarly           silenceAlarm
//                    └──────────▶ Ready ◀─────┴───────────────────────┘
//                                   │
//                                 start ──▶ Running
//
// La sesión es el día: no se "termina" a mano. closeDay() la cierra a la
// medianoche o al salir de la app.
//
// Tiempo muerto (Ready, Paused, WorkDone, BreakOver): se guarda como "pendiente".
// Si después volvés a estudiar o arrancás un recreo, era un recreo indebido.
// Si no volvés en todo el día, era el final de la jornada y cuenta como inactivo.
class SessionController : public QObject {
    Q_OBJECT
public:
    enum class State { Idle, Ready, Running, Paused, WorkDone, Break, BreakOver };
    Q_ENUM(State)

    using Clock = std::function<qint64()>; // milisegundos epoch

    explicit SessionController(Database* db, Clock clock = {}, bool autoTick = true,
                               QObject* parent = nullptr);

    State state() const { return m_state; }
    // Hay algo corriendo que no conviene cortar (pomodoro, pausa o recreo).
    bool inProgress() const;

    // ---- Configuración del próximo pomodoro / recreo --------------------
    qint64 folderId() const { return m_folderId; }
    void   setFolderId(qint64 id);
    int    workSec() const { return m_workSec; }
    void   setWorkSec(int sec);
    int    breakSec() const { return m_breakSec; }
    void   setBreakSec(int sec);

    // ---- Acciones --------------------------------------------------------
    void start();          // Idle / Ready / WorkDone / BreakOver → Running
    void pause();          // Running → Paused
    void resume();         // Paused → Running
    void endPomodoro();    // Running / Paused → WorkDone (guarda lo que llegó a correr)
    void startBreak();     // WorkDone → Break
    void skipBreak();      // WorkDone → Ready
    void endBreakEarly();  // Break → Ready
    void silenceAlarm();   // BreakOver → Ready
    void closeDay();       // * → Idle (medianoche o salir de la app)

    // Avanza el reloj. Lo llama un QTimer interno (o los tests a mano).
    void tick();

    // Al abrir la app: un pomodoro que quedó "corriendo" (cierre o corte de luz)
    // se guarda hasta donde llegó, y se retoma el tiempo pendiente de hoy.
    void recoverAfterRestart();

    // ---- Lectura para la interfaz --------------------------------------
    std::optional<Pomodoro> currentPomodoro() const;   // con el tiempo al día
    double progress() const;                           // 0..1 del pomodoro actual
    int    remainingSec() const;                       // pomodoro o recreo
    int    stateElapsedSec() const;                    // tiempo en el estado actual

signals:
    void stateChanged(SessionController::State state);
    void ticked();
    void pomodoroStarted(qint64 id);
    void pomodoroCompleted(qint64 id);          // se cumplió el tiempo (sonido de progreso)
    void pomodoroSaved(qint64 id);              // terminó (por tiempo o a mano)
    void studyRecorded(qint64 folderId, qint64 seconds); // para hitos
    void breakFinished();                       // alarma

private:
    enum class IvKind { None, Break, Pending };
    static IvKind kindFor(State s);

    qint64 now() const { return m_clock(); }
    QDate  today() const;
    void   setState(State s);
    void   openInterval(IvKind kind);
    void   closeInterval();
    void   addToInterval(qint64 ms);
    void   confirmPending();
    void   finishPomodoro();
    void   persist();

    Database* m_db;
    Clock     m_clock;
    QTimer    m_timer;

    State  m_state  = State::Idle;
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

    QVector<qint64> m_pendingIds;   // intervalos "pendientes" de hoy sin confirmar
};
