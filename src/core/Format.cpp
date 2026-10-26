#include "core/Format.h"

#include <QStringList>

namespace {
const QStringList kMonths = {"enero", "febrero", "marzo", "abril", "mayo", "junio",
                             "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"};
const QStringList kMonthsShort = {"ene", "feb", "mar", "abr", "may", "jun",
                                  "jul", "ago", "sep", "oct", "nov", "dic"};
const QStringList kDays = {"lunes", "martes", "miércoles", "jueves", "viernes", "sábado", "domingo"};
const QStringList kDaysShort = {"lun", "mar", "mié", "jue", "vie", "sáb", "dom"};
} // namespace

namespace fmt {

QString duration(qint64 s)
{
    if (s < 0) s = 0;
    const qint64 totalMin = (s + 30) / 60;
    if (totalMin < 60) return QString("%1m").arg(totalMin);
    const qint64 h = totalMin / 60;
    const qint64 m = totalMin % 60;
    if (h >= 100) return QString("%1h").arg(h);
    return QString("%1h%2").arg(h).arg(m, 2, 10, QChar('0'));
}

QString hours(qint64 s)
{
    if (s < 0) s = 0;
    const qint64 totalMin = s / 60;
    const qint64 h = totalMin / 60;
    const qint64 m = totalMin % 60;
    if (h == 0) return QString("%1m").arg(m);
    return QString("%1h %2m").arg(h).arg(m, 2, 10, QChar('0'));
}

QString clock(qint64 s)
{
    if (s < 0) s = 0;
    const qint64 h = s / 3600;
    const qint64 m = (s % 3600) / 60;
    const qint64 sec = s % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
}

QString longDate(const QDate& d)
{
    return QString("%1 %2 de %3").arg(kDays[d.dayOfWeek() - 1]).arg(d.day()).arg(kMonths[d.month() - 1]);
}

QString shortDate(const QDate& d)
{
    return QString("%1 %2").arg(kDaysShort[d.dayOfWeek() - 1]).arg(d.day());
}

QString monthYear(const QDate& d)
{
    return QString("%1 %2").arg(kMonths[d.month() - 1]).arg(d.year());
}

QString weekRange(const QDate& monday)
{
    const QDate sunday = monday.addDays(6);
    if (monday.month() == sunday.month())
        return QString("%1 – %2 %3 %4").arg(monday.day()).arg(sunday.day())
            .arg(kMonthsShort[sunday.month() - 1]).arg(sunday.year());
    return QString("%1 %2 – %3 %4 %5").arg(monday.day()).arg(kMonthsShort[monday.month() - 1])
        .arg(sunday.day()).arg(kMonthsShort[sunday.month() - 1]).arg(sunday.year());
}

QString numericDate(const QDate& d) { return d.toString(QStringLiteral("dd/MM/yy")); }

QString numericWeekRange(const QDate& monday)
{
    const QDate sunday = monday.addDays(6);
    return monday.toString(QStringLiteral("dd/MM")) + QStringLiteral(" – ") + sunday.toString(QStringLiteral("dd/MM/yy"));
}

QString numericMonth(const QDate& d) { return d.toString(QStringLiteral("MM/yyyy")); }

QString dayKey(const QDate& d) { return d.toString(Qt::ISODate); }
QDate fromDayKey(const QString& s) { return QDate::fromString(s, Qt::ISODate); }

QDate mondayOf(const QDate& d) { return d.addDays(1 - d.dayOfWeek()); }

} // namespace fmt
