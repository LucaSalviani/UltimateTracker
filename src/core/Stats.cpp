#include "core/Stats.h"
#include <QJsonArray>
#include <QJsonDocument>
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

int StreakRules::jokersOn(const QDate& d) const
{
    int n = jokers.isEmpty() ? 0 : jokers.first().second;
    for (const auto& [from, count] : jokers) {
        if (from > d) break;
        n = count;
    }
    return n;
}

StreakRules StreakRules::fixed(int jokersPerWeek)
{
    StreakRules r;
    r.jokers.append({QDate(2000, 1, 1), jokersPerWeek});
    return r;
}

StreakRules StreakRules::load(const Database& db)
{
    StreakRules r;
    const QJsonArray arr = QJsonDocument::fromJson(db.setting("jokers_history").toString().toUtf8()).array();
    for (const QJsonValue& v : arr) {
        const QJsonArray e = v.toArray();
        const QDate d = QDate::fromString(e.at(0).toString(), Qt::ISODate);
        if (d.isValid()) r.jokers.append({d, e.at(1).toInt()});
    }
    if (r.jokers.isEmpty()) r.jokers.append({QDate(2000, 1, 1), db.setting("jokers_per_week", 2).toInt()});
    r.resetDay = QDate::fromString(db.setting("streak_reset").toString(), Qt::ISODate);
    return r;
}

void setJokers(Database& db, int perWeek, const QDate& today)
{
    StreakRules r = StreakRules::load(db);
    if (r.jokersOn(today) == perWeek) { db.setSetting("jokers_per_week", perWeek); return; }
    while (!r.jokers.isEmpty() && r.jokers.last().first >= today) r.jokers.removeLast();
    if (r.jokers.isEmpty() || r.jokers.last().second != perWeek) r.jokers.append({today, perWeek});
    QJsonArray arr;
    for (const auto& [from, count] : r.jokers) arr.append(QJsonArray{from.toString(Qt::ISODate), count});
    db.setSetting("jokers_history", QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    db.setSetting("jokers_per_week", perWeek);
}

void resetStreak(Database& db, const QDate& today)
{
    db.setSetting("streak_reset", today.isValid() ? today.toString(Qt::ISODate) : QString());
}

int jokersToday(const Database& db, const QDate& today) { return StreakRules::load(db).jokersOn(today); }

Streak streak(const QSet<QDate>& studied, const QDate& today, int jokersPerWeek)
{
    return streak(studied, today, StreakRules::fixed(jokersPerWeek));
}

Streak streak(const QSet<QDate>& studied, const QDate& today, const StreakRules& rules)
{
    Streak s;
    s.todayDone = studied.contains(today);
    const QDate currentMonday = fmt::mondayOf(today);
    QDate earliestStudied;
    int jokersInWeek = 0;
    QDate weekMonday = currentMonday;
    QVector<QDate> used;

    // Recorremos hacia atrás. Hoy sin estudiar no corta ni gasta comodín.
    QDate d = s.todayDone ? today : today.addDays(-1);
    for (int guard = 0; guard < 365 * 30; ++guard, d = d.addDays(-1)) {
        if (rules.resetDay.isValid() && d < rules.resetDay) break;   // reiniciada a mano
        const QDate monday = fmt::mondayOf(d);
        if (monday != weekMonday) {
            weekMonday = monday;
            jokersInWeek = 0;
        }
        if (studied.contains(d)) {
            earliestStudied = d;
            continue;
        }
        if (jokersInWeek < rules.jokersOn(d)) {   // los comodines que regían ese día
            ++jokersInWeek;
            used.append(d);
            continue;
        }
        break; // racha cortada
    }

    if (!earliestStudied.isValid()) return s;
    // Los comodines usados antes del primer día estudiado no son parte de la racha.
    for (const QDate& x : used) {
        if (x < earliestStudied) continue;
        s.jokerDays.append(x);
        if (fmt::mondayOf(x) == currentMonday) ++s.jokersThisWeek;
    }
    // Solo suman los días con algún pomodoro; los comodines sostienen la
    // racha pero no la agrandan.
    const QDate end = s.todayDone ? today : today.addDays(-1);
    for (QDate x = earliestStudied; x <= end; x = x.addDays(1))
        if (studied.contains(x)) ++s.days;
    s.since = earliestStudied;
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
