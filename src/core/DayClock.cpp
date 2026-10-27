#include "core/DayClock.h"

#include <QtGlobal>

namespace {
int g_startHour = 4;
}

namespace dayclock {

void setStartHour(int hour) { g_startHour = qBound(0, hour, 6); }
int startHour() { return g_startHour; }

QDate logicalDate(const QDateTime& dt)
{
    return dt.addSecs(-qint64(g_startHour) * 3600).date();
}

QDate today() { return logicalDate(QDateTime::currentDateTime()); }

QDateTime dayStart(const QDate& day) { return QDateTime(day, QTime(g_startHour, 0)); }

} // namespace dayclock
