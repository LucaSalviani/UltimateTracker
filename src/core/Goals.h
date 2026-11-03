#pragma once

#include <QDate>
#include <QHash>
#include <QString>
#include <QVector>
#include <optional>

class Database;
class FolderTree;

// Metas diarias (M7). Una regla dice "qué meta hay" para un día o para varios:
//  - weekdays == 0  → solo el día `start` (también sirve para pisar una regla
//                     que se repite; con spec vacío significa "sin meta ese día").
//  - weekdays != 0  → se repite desde `start` (hasta `end` si es válido) los días
//                     marcados: bit 0 = lunes … bit 6 = domingo.
struct GoalRule {
    qint64  id = 0;
    QString title;       // texto libre de la meta (opcional)
    QString spec;        // "t:240" (por tiempo, en minutos) o "3x30+2x25" (por pomodoros);
                         // B2: cada parte puede llevar actividad: "1x25@12+3x30@7" (@ = id de la actividad)
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
    QVector<qint64> folders;     // B2: actividad de cada bloque (0 = cualquiera); mismo largo que blocks
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

// B2: cuánto de cada bloque está hecho cuando el objetivo tiene actividades.
// Primero se llenan los bloques con actividad, cada uno con lo estudiado en esa
// actividad o en sus subactividades; lo que sobra llena los bloques "cualquiera".
// Devuelve un valor 0..1 por bloque. Sin actividades da lo mismo que fillBlocks.
QVector<double> fillByActivity(const Parsed& p, const QHash<qint64, qint64>& studiedByFolder, const FolderTree& tree);
bool allDone(const QVector<double>& fills);

// Partes "3x30@7" → {3, 30, 7}. Para armar y desarmar el spec en la interfaz.
struct Part { int count = 0; int minutes = 0; qint64 folder = 0; };
QVector<Part> parts(const QString& spec);
QString joinParts(const QVector<Part>& parts);

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
