#pragma once

#include "core/Database.h"

#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QVector>

// Mejoras 4.0: números para ver la evolución en el tiempo (mapa de calor, tendencias,
// vista "Evolución"). Todo se calcula acá, sin pantalla, para poder probarlo.
namespace insights {

struct DayStat {
    qint64 study = 0;     // estudio (con el filtro de actividad, si hay)
    qint64 breaks = 0;    // recreos
    qint64 undue = 0;     // recreo indebido
    qint64 inactive = 0;  // el resto del día que no estuvo en la app
    int    pomos = 0;     // pomodoros (con tramos seguidos: valen más)
    int    sessions = 0;  // Mejoras 5.1: sesiones del día
    qint64 sessionSec = 0; // suma de lo que duraron (de que arranca el primero a que termina el último)
};

// Mejoras 5.1: una sesión va desde el primer pomodoro hasta «Me voy» o una pausa
// larga (`session_gap_min`, 60 por defecto). Cuenta en el día en que empezó.
struct Session {
    QDate  day;
    qint64 start = 0, end = 0;   // segundos epoch
    int    pomos = 0;
    qint64 study = 0;
    qint64 length() const { return end - start; }
};
QVector<Session> sessions(const Database& db, const QDate& from, const QDate& to, const QSet<qint64>* folders);

// Un día por entrada, de `from` a `to` (los futuros no aparecen). Hoy cuenta hasta `now`.
QMap<QDate, DayStat> daily(const Database& db, const QDate& from, const QDate& to,
                           const QSet<qint64>* folders, const QDateTime& now);

// Promedio móvil: cada punto es el promedio de los `window` días hasta él (menos al principio).
QVector<double> movingAverage(const QVector<double>& v, int window);

struct Trend {
    bool   enough = false;   // hay datos suficientes para opinar (3+ semanas y algo antes)
    double recent = 0;       // promedio diario de los últimos `recentDays`
    double previous = 0;     // promedio diario de los `recentDays` anteriores
    double pct = 0;          // cambio recent vs previous (0.18 = +18 %)
    double slope = 0;        // pendiente de la recta (por día), mínimos cuadrados
    double intercept = 0;
    // "Vas al alza", "Vas a la baja" o "Estás parejo" (±5 %).
    QString sentence() const;
};
// `perDay`: una serie diaria ascendente (todos los días, con 0 en los vacíos).
Trend trend(const QVector<double>& perDay, int recentDays = 28);

// Cuándo estudiás: segundos por día de la semana (0 = lunes) y hora del reloj (0–23).
struct Rhythm {
    qint64 sec[7][24] = {};
    qint64 dayTotal[7] = {};   // segundos por día de la semana
    int    dayCount[7] = {};   // cuántos de esos días hubo en el período (desde el primer día con datos)
    qint64 total = 0;
};
Rhythm rhythm(const Database& db, const QDate& from, const QDate& to, const QSet<qint64>* folders);

// Historial de la meta semanal: desde cuándo rige cada valor (no se reescribe el pasado).
void recordWeekGoal(Database& db, const QDate& monday, int goalMin);
QMap<QDate, int> weekGoals(const Database& db);   // lunes → minutos

} // namespace insights
