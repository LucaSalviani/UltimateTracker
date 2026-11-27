#include "core/Insights.h"
#include "core/DayClock.h"
#include "core/Format.h"

#include <algorithm>
#include <cmath>

namespace insights {

QMap<QDate, DayStat> daily(const Database& db, const QDate& from, const QDate& to,
                           const QSet<qint64>* folders, const QDateTime& now)
{
    QMap<QDate, DayStat> out;
    const QDate today = dayclock::logicalDate(now);
    const QDate last = qMin(to, today);
    if (from > last) return out;
    QMap<QDate, qint64> allStudy;
    for (const Pomodoro& p : db.pomodorosBetween(from, last)) {
        allStudy[p.day] += p.studiedSec;
        if (folders && !folders->contains(p.folderId)) continue;
        DayStat& d = out[p.day];
        d.study += p.studiedSec;
        d.pomos += p.doneUnits() > 0 ? p.doneUnits() : (p.status == PomoStatus::Running ? 0 : 1);
    }
    for (const Session& s : sessions(db, from, last, folders)) {
        DayStat& d = out[s.day];
        ++d.sessions;
        d.sessionSec += s.length();
    }
    for (const Interval& i : db.intervalsBetween(from, last)) {
        DayStat& d = out[i.day];
        if (i.kind == IntervalKind::Break) d.breaks += i.seconds;
        else if (i.kind == IntervalKind::Undue) d.undue += i.seconds;
    }
    for (QDate day = from; day <= last; day = day.addDays(1)) {
        DayStat& d = out[day];   // todos los días del rango existen (con ceros)
        const qint64 elapsed = day == today ? qMax<qint64>(0, dayclock::dayStart(today).secsTo(now)) : 86400;
        d.inactive = qMax<qint64>(0, elapsed - allStudy.value(day) - d.breaks - d.undue);
    }
    return out;
}

QVector<Session> sessions(const Database& db, const QDate& from, const QDate& to, const QSet<qint64>* folders)
{
    QVector<Session> out;
    QVector<Pomodoro> ps = db.pomodorosBetween(from, to);
    std::sort(ps.begin(), ps.end(), [](const Pomodoro& a, const Pomodoro& b) { return a.startedAt < b.startedAt; });
    const qint64 gap = qint64(qMax(5, db.setting("session_gap_min", 60).toInt())) * 60;
    const QVector<qint64> ends = ps.isEmpty() ? QVector<qint64>() : db.sessionEnds(ps.first().startedAt, ps.last().startedAt);
    for (const Pomodoro& p : ps) {
        if (p.status == PomoStatus::Running && p.studiedSec <= 0) continue;
        if (folders && !folders->contains(p.folderId)) continue;
        const qint64 pe = qMax(p.endedAt, p.startedAt + p.studiedSec);
        bool fresh = out.isEmpty() || out.last().day != p.day || p.startedAt - out.last().end > gap;
        if (!fresh) {
            const qint64 prevEnd = out.last().end;
            for (qint64 e : ends) if (e >= prevEnd && e <= p.startedAt) { fresh = true; break; }
        }
        if (fresh) { Session s; s.day = p.day; s.start = p.startedAt; s.end = pe; out.append(s); }
        Session& s = out.last();
        s.end = qMax(s.end, pe);
        ++s.pomos;
        s.study += p.studiedSec;
    }
    return out;
}

QVector<double> movingAverage(const QVector<double>& v, int window)
{
    QVector<double> out(v.size());
    window = qMax(1, window);
    double sum = 0;
    for (int i = 0; i < v.size(); ++i) {
        sum += v[i];
        if (i >= window) sum -= v[i - window];
        out[i] = sum / qMin(i + 1, window);
    }
    return out;
}

Rhythm rhythm(const Database& db, const QDate& from, const QDate& to, const QSet<qint64>* folders)
{
    Rhythm r;
    QDate first = to;
    for (const Pomodoro& p : db.pomodorosBetween(from, to)) {
        if (p.status == PomoStatus::Running && p.studiedSec <= 0) continue;
        if (folders && !folders->contains(p.folderId)) continue;
        if (p.studiedSec <= 0) continue;
        first = qMin(first, p.day);
        const int wd = p.day.dayOfWeek() - 1;
        qint64 t = p.startedAt;
        const qint64 end = p.startedAt + p.studiedSec;
        while (t < end) {
            const QDateTime dt = QDateTime::fromSecsSinceEpoch(t);
            const qint64 nextHour = QDateTime(dt.date(), QTime(dt.time().hour(), 0)).addSecs(3600).toSecsSinceEpoch();
            const qint64 chunk = qMin(end, nextHour) - t;
            r.sec[wd][dt.time().hour()] += chunk;
            r.dayTotal[wd] += chunk;
            r.total += chunk;
            t += chunk;
        }
    }
    if (r.total > 0)
        for (QDate d = first; d <= to; d = d.addDays(1)) ++r.dayCount[d.dayOfWeek() - 1];
    return r;
}

Trend trend(const QVector<double>& perDay, int recentDays)
{
    Trend t;
    const int n = int(perDay.size());
    if (n == 0) return t;
    // Recta de mínimos cuadrados sobre toda la serie.
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < n; ++i) { sx += i; sy += perDay[i]; sxx += double(i) * i; sxy += i * perDay[i]; }
    const double den = n * sxx - sx * sx;
    t.slope = den != 0 ? (n * sxy - sx * sy) / den : 0;
    t.intercept = (sy - t.slope * sx) / n;
    // Últimos días contra los anteriores.
    const int r = qMin(recentDays, n);
    double rec = 0;
    for (int i = n - r; i < n; ++i) rec += perDay[i];
    t.recent = rec / qMax(1, r);
    const int pr = qMin(recentDays, n - r);
    if (pr > 0) {
        double prev = 0;
        for (int i = n - r - pr; i < n - r; ++i) prev += perDay[i];
        t.previous = prev / pr;
    }
    t.enough = r >= 7 && pr >= qMin(7, recentDays) && t.previous > 0;
    t.pct = t.previous > 0 ? (t.recent - t.previous) / t.previous : 0;
    return t;
}

QString Trend::sentence() const
{
    if (!enough) return QStringLiteral("Todavía no hay datos suficientes para una tendencia (hace falta al menos una semana de estudio antes del período).");
    const int p = int(std::lround(std::abs(pct) * 100));
    if (std::abs(pct) < 0.05) return QStringLiteral("Estás parejo: casi lo mismo que el período anterior.");
    return pct > 0 ? QStringLiteral("Vas al alza: +%1 % que el período anterior.").arg(p)
                   : QStringLiteral("Vas a la baja: −%1 % que el período anterior.").arg(p);
}

void recordWeekGoal(Database& db, const QDate& monday, int goalMin)
{
    QMap<QDate, int> m = weekGoals(db);
    if (m.value(monday, -1) == goalMin) return;
    m[monday] = goalMin;
    QStringList parts;
    for (auto it = m.cbegin(); it != m.cend(); ++it) parts << QStringLiteral("%1:%2").arg(fmt::dayKey(it.key())).arg(it.value());
    db.setSetting("week_goal_history", parts.join(QLatin1Char(',')));
}

QMap<QDate, int> weekGoals(const Database& db)
{
    QMap<QDate, int> out;
    for (const QString& s : db.setting("week_goal_history").toString().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const auto kv = s.split(QLatin1Char(':'));
        if (kv.size() == 2) out.insert(fmt::fromDayKey(kv[0]), kv[1].toInt());
    }
    return out;
}

} // namespace insights
