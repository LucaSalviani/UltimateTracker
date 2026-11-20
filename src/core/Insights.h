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
};

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

// Historial de la meta semanal: desde cuándo rige cada valor (no se reescribe el pasado).
void recordWeekGoal(Database& db, const QDate& monday, int goalMin);
QMap<QDate, int> weekGoals(const Database& db);   // lunes → minutos

} // namespace insights
