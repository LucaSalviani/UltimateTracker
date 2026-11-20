#include "core/Insights.h"
#include "core/DayClock.h"
#include "core/Format.h"

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
    t.enough = n >= 21 && pr >= 14 && t.previous > 0;
    t.pct = t.previous > 0 ? (t.recent - t.previous) / t.previous : 0;
    return t;
}

QString Trend::sentence() const
{
    if (!enough) return QStringLiteral("Todavía no hay datos suficientes para una tendencia (hacen falta unas 3 semanas).");
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
