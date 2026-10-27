#pragma once

#include <QDate>
#include <QMap>
#include <QString>
#include <QVector>

class Database;

// M44: hitos en tres plazos.
//  · Corto (todos los días): marca del día (bronce / plata / oro), rachas, récords
//    personales y meta semanal.
//  · Mediano (semanas y meses): horas por actividad con una curva que se estira,
//    semanas seguidas cumpliendo la meta y el resumen del mes.
//  · Largo: etapas camino a las 10.000 h (por actividad y un nivel global).
// Todo se puede prender o apagar por separado (menú ⋯ › Hitos).
namespace ms {

struct Config {
    bool dayMark = true;
    bool streaks = true;    // rachas y récords
    bool weekly  = true;
    bool hours   = true;    // horas por actividad
    bool stages  = true;    // etapas hacia las 10.000 h
    bool month   = true;    // resumen del mes
    int  markMin[3] = {120, 240, 360};   // bronce, plata, oro (minutos en el día)
    int  weeklyMin = 20 * 60;
    static Config load(const Database& db);
};

// ---- Largo plazo: etapas
struct Stage {
    QString name;      // "Practicante"
    QString label;     // "Practicante II"
    int     index = 0; // 0 = Novato … 7 = Maestro
    int     sub = 0;   // 1..3 (0 si la etapa no tiene subniveles)
    qint64  fromSec = 0, toSec = 0;   // tramo del subnivel actual (toSec = 0 → no hay más)
    double  progress = 0;             // 0..1 dentro del subnivel
};
Stage stageFor(qint64 seconds);
QStringList stageNames();

// ---- Mediano plazo: horas por actividad
QVector<int> hourMarks();                                   // 1, 5, 10, 25, 50, 100, …, 10.000
QVector<int> hoursCrossed(qint64 beforeSec, qint64 afterSec);

// ---- Corto plazo
int dayMark(qint64 seconds, const int markMin[3]);          // 0 nada, 1 bronce, 2 plata, 3 oro
QString dayMarkName(int level);                             // "bronce", "plata", "oro"
QVector<int> streakMarks();                                 // 3, 7, 14, 30, 60, 100, 180, 365
QVector<int> weekStreakMarks();                             // 2, 4, 8, 12, 26, 52

struct Records {
    qint64 bestDay = 0;  QDate bestDayDate;
    qint64 bestWeek = 0; QDate bestWeekMonday;
};
// Récords sobre los días anteriores a `before` (para saber si hoy los supera).
Records records(const QMap<QDate, qint64>& byDay, const QDate& before);
qint64 weekTotal(const QMap<QDate, qint64>& byDay, const QDate& anyDayOfWeek);
// Semanas completas seguidas (terminando en la anterior a `today`) que llegaron
// a la meta; si la semana actual ya llegó, también cuenta.
int weeksInARow(const QMap<QDate, qint64>& byDay, const QDate& today, qint64 weeklySec);

QString roman(int n);

} // namespace ms
