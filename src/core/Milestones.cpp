#include "core/Milestones.h"
#include "core/Database.h"
#include "core/Format.h"

namespace ms {

Config Config::load(const Database& db)
{
    Config c;
    c.dayMark = db.setting("ms_day", 1).toBool();
    c.streaks = db.setting("ms_streaks", 1).toBool();
    c.weekly  = db.setting("ms_weekly", 1).toBool();
    c.hours   = db.setting("ms_hours", 1).toBool();
    c.stages  = db.setting("ms_stages", 1).toBool();
    c.month   = db.setting("ms_month", 1).toBool();
    c.markMin[0] = db.setting("ms_bronze_min", 120).toInt();
    c.markMin[1] = qMax(c.markMin[0], db.setting("ms_silver_min", 240).toInt());
    c.markMin[2] = qMax(c.markMin[1], db.setting("ms_gold_min", 360).toInt());
    c.weeklyMin = db.setting("ms_weekly_min", 20 * 60).toInt();
    return c;
}

// ---------------------------------------------------------------------------
namespace {
struct StageDef { const char* name; int fromH, toH, subs; };
// El primer corte en 20 h: lo que tarda cualquiera en "salir de cero" (Kaufman).
constexpr StageDef kStages[] = {
    {"Novato", 0, 20, 0},
    {"Aprendiz", 20, 100, 3},
    {"Practicante", 100, 500, 3},
    {"Competente", 500, 1000, 3},
    {"Avanzado", 1000, 2500, 3},
    {"Especialista", 2500, 5000, 3},
    {"Experto", 5000, 10000, 3},
    {"Maestro", 10000, 0, 0},
};
} // namespace

QString roman(int n)
{
    static const char* r[] = {"", "I", "II", "III", "IV", "V"};
    return n >= 0 && n <= 5 ? QString::fromLatin1(r[n]) : QString::number(n);
}

QStringList stageNames()
{
    QStringList out;
    for (const auto& s : kStages) out << QString::fromUtf8(s.name);
    return out;
}

Stage stageFor(qint64 seconds)
{
    const double h = seconds / 3600.0;
    Stage st;
    for (int i = 0; i < int(std::size(kStages)); ++i) {
        const StageDef& d = kStages[i];
        if (d.toH != 0 && h >= d.toH) continue;
        st.name = QString::fromUtf8(d.name);
        st.index = i;
        if (d.toH == 0) {   // Maestro
            st.label = st.name;
            st.fromSec = qint64(d.fromH) * 3600;
            st.progress = 1;
            return st;
        }
        if (d.subs == 0) {
            st.fromSec = qint64(d.fromH) * 3600;
            st.toSec = qint64(d.toH) * 3600;
        } else {
            const double span = double(d.toH - d.fromH) / d.subs;
            st.sub = qBound(1, int((h - d.fromH) / span) + 1, d.subs);
            st.fromSec = qint64((d.fromH + span * (st.sub - 1)) * 3600);
            st.toSec = qint64((d.fromH + span * st.sub) * 3600);
        }
        st.label = st.sub ? st.name + QLatin1Char(' ') + roman(st.sub) : st.name;
        st.progress = qBound(0.0, double(seconds - st.fromSec) / qMax<qint64>(1, st.toSec - st.fromSec), 1.0);
        return st;
    }
    return st;
}

// ---------------------------------------------------------------------------
QVector<int> hourMarks()
{
    static const QVector<int> marks = [] {
        QVector<int> m{1, 5, 10, 25, 50, 100, 150, 200, 300, 400, 500, 750, 1000};
        for (int h = 1500; h <= 10000; h += 500) m.append(h);
        return m;
    }();
    return marks;
}

QVector<int> hoursCrossed(qint64 beforeSec, qint64 afterSec)
{
    QVector<int> out;
    if (afterSec <= beforeSec) return out;
    for (int h : hourMarks())
        if (beforeSec < qint64(h) * 3600 && afterSec >= qint64(h) * 3600) out.append(h);
    return out;
}

int dayMark(qint64 seconds, const int markMin[3])
{
    int level = 0;
    for (int i = 0; i < 3; ++i)
        if (markMin[i] > 0 && seconds >= qint64(markMin[i]) * 60) level = i + 1;
    return level;
}

QString dayMarkName(int level)
{
    switch (level) {
    case 1: return QStringLiteral("bronce");
    case 2: return QStringLiteral("plata");
    case 3: return QStringLiteral("oro");
    default: return QString();
    }
}

QVector<int> streakMarks() { return {3, 7, 14, 30, 60, 100, 180, 365}; }
QVector<int> weekStreakMarks() { return {2, 4, 8, 12, 26, 52}; }

Records records(const QMap<QDate, qint64>& byDay, const QDate& before)
{
    Records r;
    QMap<QDate, qint64> weeks;
    for (auto it = byDay.cbegin(); it != byDay.cend(); ++it) {
        if (it.key() >= before) continue;
        if (it.value() > r.bestDay) { r.bestDay = it.value(); r.bestDayDate = it.key(); }
        weeks[fmt::mondayOf(it.key())] += it.value();
    }
    const QDate thisMonday = fmt::mondayOf(before);
    for (auto it = weeks.cbegin(); it != weeks.cend(); ++it) {
        if (it.key() >= thisMonday) continue;   // la semana en curso no cuenta como récord previo
        if (it.value() > r.bestWeek) { r.bestWeek = it.value(); r.bestWeekMonday = it.key(); }
    }
    return r;
}

qint64 weekTotal(const QMap<QDate, qint64>& byDay, const QDate& anyDayOfWeek)
{
    const QDate monday = fmt::mondayOf(anyDayOfWeek);
    qint64 t = 0;
    for (int i = 0; i < 7; ++i) t += byDay.value(monday.addDays(i));
    return t;
}

int weeksInARow(const QMap<QDate, qint64>& byDay, const QDate& today, qint64 weeklySec)
{
    if (weeklySec <= 0) return 0;
    int n = weekTotal(byDay, today) >= weeklySec ? 1 : 0;
    for (QDate m = fmt::mondayOf(today).addDays(-7); n < 1000; m = m.addDays(-7)) {
        if (weekTotal(byDay, m) < weeklySec) break;
        ++n;
    }
    return n;
}

} // namespace ms
