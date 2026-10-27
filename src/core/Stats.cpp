#include "core/Stats.h"
#include "core/DayClock.h"
#include "core/Format.h"

namespace stats {

Totals totals(const Database& db, const QDate& from, const QDate& to,
              const QSet<qint64>* folders, const QDateTime& now)
{
    Totals t;
    for (const Pomodoro& p : db.pomodorosBetween(from, to)) {
        if (folders && !folders->contains(p.folderId)) continue;
        t.study += p.studiedSec;
    }
    for (const Interval& i : db.intervalsBetween(from, to)) {
        if (i.kind == IntervalKind::Break)      t.breaks += i.seconds;
        else if (i.kind == IntervalKind::Undue) t.undue  += i.seconds;
        // Pending = tiempo que terminó siendo el final del día: cuenta como inactivo.
    }

    // Horas inactivas = tiempo transcurrido del período − lo usado en la app.
    // Con filtro de carpeta, lo que se estudió en otras carpetas no es "inactivo",
    // así que se calcula siempre contra el estudio total.
    const QDate today = dayclock::logicalDate(now);
    const QDate last = qMin(to, today);
    if (from <= last) {
        qint64 elapsed = qint64(from.daysTo(last)) * 86400;           // días completos
        elapsed += (last == today) ? dayclock::dayStart(today).secsTo(now)   // hoy: hasta ahora
                                   : 86400;
        qint64 allStudy = t.study;
        if (folders) {
            allStudy = 0;
            for (const Pomodoro& p : db.pomodorosBetween(from, to)) allStudy += p.studiedSec;
        }
        t.inactive = qMax<qint64>(0, elapsed - allStudy - t.breaks - t.undue);
    }
    return t;
}

QMap<QDate, qint64> studyByDay(const Database& db, const QDate& from, const QDate& to,
                               const QSet<qint64>* folders)
{
    QMap<QDate, qint64> out;
    for (const Pomodoro& p : db.pomodorosBetween(from, to)) {
        if (folders && !folders->contains(p.folderId)) continue;
        out[p.day] += p.studiedSec;
    }
    return out;
}

Streak streak(const QSet<QDate>& studied, const QDate& today, int jokersPerWeek)
{
    Streak s;
    s.todayDone = studied.contains(today);
    if (studied.isEmpty()) return s;

    QDate earliestStudied;
    QDate currentMonday = fmt::mondayOf(today);
    int jokersInWeek = 0;
    QDate weekMonday = currentMonday;

    // Recorremos hacia atrás. Hoy sin estudiar no corta ni gasta comodín.
    QDate d = s.todayDone ? today : today.addDays(-1);
    for (int guard = 0; guard < 365 * 30; ++guard, d = d.addDays(-1)) {
        const QDate monday = fmt::mondayOf(d);
        if (monday != weekMonday) {
            weekMonday = monday;
            jokersInWeek = 0;
        }
        if (studied.contains(d)) {
            earliestStudied = d;
            continue;
        }
        if (jokersInWeek < jokersPerWeek) {
            ++jokersInWeek;
            if (monday == currentMonday) ++s.jokersThisWeek;
            continue;
        }
        break; // racha cortada
    }

    if (!earliestStudied.isValid()) {
        s.jokersThisWeek = 0;
        return s;
    }
    // Los comodines usados antes del primer día estudiado no son parte de la racha.
    if (fmt::mondayOf(earliestStudied) == currentMonday) {
        int used = 0;
        for (QDate x = earliestStudied; x < today; x = x.addDays(1))
            if (!studied.contains(x)) ++used;
        s.jokersThisWeek = used;
    }
    const QDate end = s.todayDone ? today : today.addDays(-1);
    s.days = int(earliestStudied.daysTo(end)) + 1;
    if (!s.todayDone && s.days < 0) s.days = 0;
    return s;
}

QVector<int> milestonesCrossed(qint64 beforeSec, qint64 afterSec)
{
    QVector<int> out;
    if (afterSec <= beforeSec) return out;
    auto crossed = [&](int h) { return beforeSec < qint64(h) * 3600 && afterSec >= qint64(h) * 3600; };
    for (int h = 1; h <= 10; ++h)
        if (crossed(h)) out.append(h);
    const qint64 firstK = beforeSec / (25 * 3600) + 1;
    for (qint64 k = firstK; k * 25 * 3600 <= afterSec; ++k)
        out.append(int(k * 25));
    return out;
}

} // namespace stats
