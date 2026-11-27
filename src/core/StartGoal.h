#pragma once

#include "core/Database.h"

#include <QDateTime>
#include <QVector>
#include <optional>

// Mejoras 7.0: objetivo de arranque. Para un día: a qué hora arrancás y una tarea mínima
// (N pomodoros, de una actividad o de cualquiera). Se define al irte («Me voy») o desde el menú,
// y se mide: cumplido si el primer pomodoro empieza hasta `kToleranceMin` después de la hora
// y se hizo lo mínimo. Solo local (no se sincroniza).
namespace startgoal {

constexpr int kToleranceMin = 15;

struct Goal {
    QDate   day;
    int     minute = 8 * 60;   // hora de arranque, minutos desde las 00:00
    qint64  folderId = 0;      // 0 = cualquiera
    int     pomos = 1;         // mínimo de pomodoros
};

struct Result {
    bool      pending = false;   // todavía hay tiempo de cumplirlo (hoy, antes de la hora + tolerancia)
    bool      started = false;
    QDateTime firstStart;        // primer pomodoro del día
    int       lateMin = 0;       // cuánto tarde arrancó (0 si a tiempo)
    bool      onTime = false;
    int       done = 0;          // pomodoros hechos de lo pedido
    bool      taskDone = false;
    bool      met = false;       // a tiempo y con lo mínimo hecho
};

std::optional<Goal> get(const Database& db, const QDate& day);
void save(Database& db, const Goal& g);
void remove(Database& db, const QDate& day);
QVector<Goal> between(const Database& db, const QDate& from, const QDate& to);

QDateTime when(const Goal& g);   // la hora de arranque como fecha y hora (cuenta el día lógico)
Result evaluate(const Database& db, const Goal& g, const QDateTime& now);

} // namespace startgoal
