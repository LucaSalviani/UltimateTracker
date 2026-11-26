#pragma once

#include "core/Database.h"

#include <QDate>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

// Mejoras 5.1: rutina = actividades de tu día por fuera de la app (facultad,
// gimnasio, médico…). No avisan nada: sirven para explicar las horas inactivas
// ("inactivo 9 h: 4 h facultad, 1 h gimnasio, 4 h sin motivo").
namespace routine {

struct Event {
    qint64  id = 0;
    QString title;
    QString category;       // "Facultad", "Gimnasio"… (texto libre)
    int     startMin = 0;   // minutos desde las 00:00
    int     endMin = 0;     // si es menor que el inicio, cruza la medianoche
    int     weekdays = 0;   // bits: lunes = 1 … domingo = 64. 0 = un solo día (`day`)
    QDate   day;            // solo cuando weekdays == 0
    QDate   from, to;       // vigencia de lo que se repite (inválida = sin límite)
};

QStringList defaultCategories();
QString weekdaysText(int mask);   // "Lun a Vie", "Lun, Mié", "Todos los días"

QVector<Event> events(const Database& db);
qint64 save(Database& db, const Event& e);   // id == 0 inserta
void   remove(Database& db, qint64 id);

struct Occurrence { const Event* ev; qint64 start, end; };   // segundos epoch
// Lo que cae dentro del día de la app `day` (de dayStart a dayStart + 24 h).
QVector<Occurrence> occurrences(const QVector<Event>& evs, const QDate& day);

// Mejoras 6.0: lo que cae en la fecha de calendario `cal` (para mostrar en semana y mes), ordenado por hora.
QVector<Event> onDate(const QVector<Event>& evs, const QDate& cal);

struct Explain {
    qint64 total = 0;                  // horas inactivas del día
    QMap<QString, qint64> byCategory;  // lo que explica la rutina
    qint64 unexplained = 0;            // "sin motivo"
};
// Reparte las horas inactivas de `day` (hasta `now`) entre las actividades de la rutina.
Explain explain(const Database& db, const QVector<Event>& evs, const QDate& day, const QDateTime& now);

} // namespace routine
