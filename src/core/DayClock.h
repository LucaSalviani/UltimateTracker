#pragma once

#include <QDate>
#include <QDateTime>

// El "día" de la app no empieza a la medianoche sino a una hora configurable
// (por defecto 4 AM), así una sesión nocturna que pasa las 00:00 sigue
// contando para el día en que empezaste.
namespace dayclock {

void      setStartHour(int hour);          // 0..6
int       startHour();
QDate     logicalDate(const QDateTime& dt); // a qué día de la app pertenece un instante
QDate     today();
QDateTime dayStart(const QDate& day);      // cuándo empieza ese día (ej. 27/10 04:00)

} // namespace dayclock
