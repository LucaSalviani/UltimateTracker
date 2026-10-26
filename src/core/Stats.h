#pragma once

#include "core/Database.h"

#include <QDateTime>
#include <QMap>
#include <QSet>

namespace stats {

struct Totals {
    qint64 study    = 0;
    qint64 breaks   = 0;
    qint64 undue    = 0;
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
    int  days            = 0;      // días corridos (incluye comodines en el medio)
    int  jokersThisWeek  = 0;      // comodines gastados en la semana actual
    bool todayDone       = false;  // ya hay al menos un pomodoro hoy
};

// Racha: un día cuenta si tiene al menos un pomodoro. Cada semana (lun–dom)
// permite `jokersPerWeek` días sin estudio sin cortar la racha.
// Hoy, si todavía no se estudió, no corta la racha (el día no terminó).
Streak streak(const QSet<QDate>& studied, const QDate& today, int jokersPerWeek);

// Hitos de horas cruzados al pasar de `before` a `after` segundos:
// 1, 2, …, 10 h y luego cada 25 h (25, 50, 75, …).
QVector<int> milestonesCrossed(qint64 beforeSec, qint64 afterSec);

} // namespace stats
