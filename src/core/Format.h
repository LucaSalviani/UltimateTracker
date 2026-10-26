#pragma once

#include <QDate>
#include <QString>

namespace fmt {

// 25m  ·  1h05  ·  132h
QString duration(qint64 seconds);
// 3h 05m  (para totales grandes)
QString hours(qint64 seconds);
// 18:32  (cuenta regresiva)
QString clock(qint64 seconds);
// "lunes 26 de octubre"
QString longDate(const QDate& d);
// "lun 26"
QString shortDate(const QDate& d);
// "octubre 2026"
QString monthYear(const QDate& d);
// "26 oct – 1 nov 2026"
QString weekRange(const QDate& monday);

// Versiones numéricas: 26/10/26 · 26/10 – 01/11/26 · 10/2026
QString numericDate(const QDate& d);
QString numericWeekRange(const QDate& monday);
QString numericMonth(const QDate& d);

QString dayKey(const QDate& d);   // 2026-10-26
QDate   fromDayKey(const QString& s);

QDate mondayOf(const QDate& d);

} // namespace fmt
