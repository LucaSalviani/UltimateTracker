#pragma once

#include "core/Database.h"

#include <QDate>
#include <QString>
#include <QVector>

// Mejoras 7.0: uso de programas en la PC. Cada tanto se anota qué ventana estaba al frente
// (programa + título) y cuántos segundos, separando lo que pasó con un pomodoro corriendo de
// lo que pasó fuera. Todo local (no se sincroniza ni se exporta a ningún lado), apagado por defecto.
namespace appusage {

constexpr int kIdleSec = 180;   // sin teclado ni mouse tanto tiempo = no cuenta

struct Row {
    QString exe;
    qint64  pomoSec = 0;   // con un pomodoro corriendo
    qint64  outSec = 0;    // fuera de un pomodoro
    qint64  total() const { return pomoSec + outSec; }
};

void add(Database& db, const QDate& day, const QString& exe, const QString& title, bool inPomodoro, qint64 sec);
QVector<Row> byApp(const Database& db, const QDate& from, const QDate& to);                       // de más a menos
QVector<QPair<QString, qint64>> titles(const Database& db, const QDate& from, const QDate& to, const QString& exe, int limit = 8);
bool any(const Database& db);
void clear(Database& db);

} // namespace appusage
