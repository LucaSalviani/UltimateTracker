#pragma once

#include <QDate>
#include <QString>
#include <QVector>
#include <optional>

class Database;

// Metas diarias (M7). Una regla dice "qué meta hay" para un día o para varios:
//  - weekdays == 0  → solo el día `start` (también sirve para pisar una regla
//                     que se repite; con spec vacío significa "sin meta ese día").
//  - weekdays != 0  → se repite desde `start` (hasta `end` si es válido) los días
//                     marcados: bit 0 = lunes … bit 6 = domingo.
struct GoalRule {
    qint64  id = 0;
    QString title;       // texto libre de la meta (opcional)
    QString spec;        // "t:240" (por tiempo, en minutos) o "3x30+2x25" (por pomodoros)
    QDate   start;
    QDate   end;         // inválida = sin fin
    int     weekdays = 0;
};

namespace goals {

constexpr int kEveryDay = 0x7f;
constexpr int kWeekdays = 0x1f;   // lunes a viernes

struct Parsed {
    bool         ok = false;
    bool         timeMode = false;
    int          totalMin = 0;   // minutos de la meta
    QVector<int> blocks;         // minutos de cada cuadradito fantasma
};

// Interpreta el spec guardado. Por tiempo, los cuadraditos se calculan con la
// duración de pomodoro que estés usando (redondeando para arriba).
Parsed parse(const QString& spec, int pomodoroMin);

// Lo que escribe el usuario en "por pomodoros": "3x30 + 2x25", "3×30", "25, 25, 50",
// "1h" (= un bloque de 60). Devuelve el spec normalizado ("3x30+2x25") o vacío si no se entiende.
QString normalizeCombo(const QString& text);

// "3×30 + 2×25" / "4h" para mostrar.
QString describe(const QString& spec);

// M21: cuánto del objetivo está hecho, por TIEMPO acumulado: los minutos
// estudiados van llenando los bloques en orden; el que queda a medias, en parte.
struct BlockFill {
    int    full = 0;       // bloques completos
    double partial = 0;    // 0..1 del siguiente
};
BlockFill fillBlocks(const QVector<int>& blocksMin, qint64 studiedSec);

bool appliesOn(const GoalRule& r, const QDate& d);
// La regla que manda ese día (una puntual le gana a una que se repite; entre
// iguales, la más nueva). nullopt = no hay regla.
std::optional<GoalRule> resolve(const QVector<GoalRule>& rules, const QDate& d);

// Guardar desde el panel. weekdays == 0 → solo ese día; si no, se repite desde
// ese día (la regla anterior que se repetía queda cortada el día antes, así el
// pasado no cambia).
void save(Database& db, const QDate& d, const QString& title, const QString& spec, int weekdays);

// Quitar: solo ese día, o la regla que se repite desde ese día en adelante.
void removeDay(Database& db, const QDate& d);
void removeFromDay(Database& db, const QDate& d);

} // namespace goals
