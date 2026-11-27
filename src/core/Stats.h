#pragma once

#include "core/Database.h"

#include <QDateTime>
#include <QMap>
#include <QSet>

namespace stats {

struct Totals {
    qint64 study    = 0;
    qint64 breaks   = 0;
    qint64 undue    = 0;   // «tiempo muerto» (entre pomodoros, recreo pasado de tiempo)
    qint64 paused   = 0;   // Mejoras 7.0: «recreo indebido» = pausas con un pomodoro en curso
    qint64 inactive = 0;
};

// Totales del período [from, to]. Si folders != nullptr, el estudio se filtra
// a esas carpetas (los recreos son globales). Los días futuros no cuentan y
// hoy cuenta solo hasta `now` para las horas inactivas.
Totals totals(const Database& db, const QDate& from, const QDate& to,
              const QSet<qint64>* folders, const QDateTime& now);

// Estudio por día dentro del período.
QMap<QDate, qint64> studyByDay(const Database& db, const QDate& from, const QDate& to,
                               const QSet<qint64>* folders);

struct Streak {
    int  days            = 0;      // días con pomodoro de la racha (los comodines no suman)
    int  jokersThisWeek  = 0;      // comodines gastados en la semana actual
    bool todayDone       = false;  // ya hay al menos un pomodoro hoy
    QDate since;                   // primer día de la racha
    QVector<QDate> jokerDays;      // días sin estudio que cubrió un comodín
};

// Mejoras 3.0: las reglas de la racha no son retroactivas.
//  - Cambiar la cantidad de comodines vale desde ese día en adelante
//    (se guarda el historial: desde qué día rige cada cantidad).
//  - La racha se puede reiniciar a mano: cuenta desde ese día.
struct StreakRules {
    QVector<QPair<QDate, int>> jokers;   // ordenado por fecha: (desde, comodines por semana)
    QDate resetDay;                      // inválida = nunca se reinició
    int jokersOn(const QDate& d) const;
    static StreakRules fixed(int jokersPerWeek);
    static StreakRules load(const Database& db);
};
// Cambia los comodines por semana desde `today` (lo anterior queda como estaba).
void setJokers(Database& db, int perWeek, const QDate& today);
// Reinicia la racha: cuenta desde `today`. Con día inválido, deshace el reinicio.
void resetStreak(Database& db, const QDate& today);
int  jokersToday(const Database& db, const QDate& today);

Streak streak(const QSet<QDate>& studied, const QDate& today, const StreakRules& rules);

// Racha: un día cuenta si tiene al menos un pomodoro. Cada semana (lun–dom)
// permite `jokersPerWeek` días sin estudio sin cortar la racha.
// Hoy, si todavía no se estudió, no corta la racha (el día no terminó).
Streak streak(const QSet<QDate>& studied, const QDate& today, int jokersPerWeek);

// Hitos de horas cruzados al pasar de `before` a `after` segundos:
// 1, 2, …, 10 h y luego cada 25 h (25, 50, 75, …).
QVector<int> milestonesCrossed(qint64 beforeSec, qint64 afterSec);

} // namespace stats
