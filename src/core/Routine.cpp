#include "core/Routine.h"
#include "core/DayClock.h"
#include "core/Format.h"

#include <QSqlQuery>
#include <QSqlError>
#include <algorithm>

namespace routine {

QStringList defaultCategories()
{
    return {QStringLiteral("Facultad"), QStringLiteral("Trabajo"), QStringLiteral("Gimnasio"), QStringLiteral("Médico"),
            QStringLiteral("Traslado"), QStringLiteral("Comida"), QStringLiteral("Otro")};
}

QString weekdaysText(int mask)
{
    static const char* n[] = {"Lun", "Mar", "Mié", "Jue", "Vie", "Sáb", "Dom"};
    if ((mask & 127) == 127) return QStringLiteral("Todos los días");
    if ((mask & 31) == 31 && !(mask & 96)) return QStringLiteral("Lun a Vie");
    QStringList l;
    for (int i = 0; i < 7; ++i) if (mask & (1 << i)) l << QString::fromUtf8(n[i]);
    return l.join(QStringLiteral(", "));
}

QVector<Event> events(const Database& db)
{
    QVector<Event> out;
    QSqlQuery q(db.handle());
    q.exec("SELECT id, title, category, start_min, end_min, weekdays, day, from_day, to_day FROM routine_events ORDER BY start_min, id");
    while (q.next()) {
        Event e;
        e.id = q.value(0).toLongLong();
        e.title = q.value(1).toString();
        e.category = q.value(2).toString();
        e.startMin = q.value(3).toInt();
        e.endMin = q.value(4).toInt();
        e.weekdays = q.value(5).toInt();
        if (!q.value(6).toString().isEmpty()) e.day = fmt::fromDayKey(q.value(6).toString());
        if (!q.value(7).toString().isEmpty()) e.from = fmt::fromDayKey(q.value(7).toString());
        if (!q.value(8).toString().isEmpty()) e.to = fmt::fromDayKey(q.value(8).toString());
        out.append(e);
    }
    return out;
}

qint64 save(Database& db, const Event& e)
{
    QSqlQuery q(db.handle());
    if (e.id) q.prepare("UPDATE routine_events SET title=?, category=?, start_min=?, end_min=?, weekdays=?, day=?, from_day=?, to_day=? WHERE id=?");
    else q.prepare("INSERT INTO routine_events(title, category, start_min, end_min, weekdays, day, from_day, to_day) VALUES(?,?,?,?,?,?,?,?)");
    q.addBindValue(e.title);
    q.addBindValue(e.category);
    q.addBindValue(e.startMin);
    q.addBindValue(e.endMin);
    q.addBindValue(e.weekdays);
    q.addBindValue(e.day.isValid() ? fmt::dayKey(e.day) : QStringLiteral(""));
    q.addBindValue(e.from.isValid() ? fmt::dayKey(e.from) : QStringLiteral(""));
    q.addBindValue(e.to.isValid() ? fmt::dayKey(e.to) : QStringLiteral(""));
    if (e.id) q.addBindValue(e.id);
    if (!q.exec()) qWarning("routine save: %s", qPrintable(q.lastError().text()));
    return e.id ? e.id : q.lastInsertId().toLongLong();
}

void remove(Database& db, qint64 id)
{
    QSqlQuery q(db.handle());
    q.prepare("DELETE FROM routine_events WHERE id = ?");
    q.addBindValue(id);
    q.exec();
}

QVector<Occurrence> occurrences(const QVector<Event>& evs, const QDate& day)
{
    QVector<Occurrence> out;
    const qint64 w0 = dayclock::dayStart(day).toSecsSinceEpoch();
    const qint64 w1 = dayclock::dayStart(day.addDays(1)).toSecsSinceEpoch();
    // Fechas de calendario que tocan la ventana (puede empezar el día anterior si el día de la app arranca a las 4).
    for (const QDate& cal : {day.addDays(-1), day, day.addDays(1)}) {
        for (const Event& e : evs) {
            bool on;
            if (e.weekdays == 0) on = e.day == cal;
            else on = (e.weekdays & (1 << (cal.dayOfWeek() - 1))) && (!e.from.isValid() || cal >= e.from) && (!e.to.isValid() || cal <= e.to);
            if (!on) continue;
            qint64 s = QDateTime(cal, QTime(0, 0)).toSecsSinceEpoch() + qint64(e.startMin) * 60;
            qint64 f = QDateTime(cal, QTime(0, 0)).toSecsSinceEpoch() + qint64(e.endMin) * 60;
            if (e.endMin <= e.startMin) f = QDateTime(cal.addDays(1), QTime(0, 0)).toSecsSinceEpoch() + qint64(e.endMin) * 60;
            s = qMax(s, w0);
            f = qMin(f, w1);
            if (f > s) out.append({&e, s, f});
        }
    }
    std::sort(out.begin(), out.end(), [](const Occurrence& a, const Occurrence& b) { return a.start < b.start; });
    return out;
}

namespace {
using Span = QPair<qint64, qint64>;
// Resta [s, f] de una lista de tramos libres y devuelve cuántos segundos se cubrieron.
qint64 claim(QVector<Span>& free, qint64 s, qint64 f)
{
    qint64 got = 0;
    QVector<Span> next;
    for (const Span& sp : free) {
        const qint64 a = qMax(sp.first, s), b = qMin(sp.second, f);
        if (b <= a) { next.append(sp); continue; }
        got += b - a;
        if (a > sp.first) next.append({sp.first, a});
        if (b < sp.second) next.append({b, sp.second});
    }
    free = next;
    return got;
}
} // namespace

Explain explain(const Database& db, const QVector<Event>& evs, const QDate& day, const QDateTime& now)
{
    Explain out;
    const qint64 w0 = dayclock::dayStart(day).toSecsSinceEpoch();
    const qint64 w1 = qMin(dayclock::dayStart(day.addDays(1)).toSecsSinceEpoch(), now.toSecsSinceEpoch());
    if (w1 <= w0) return out;
    QVector<Span> free{{w0, w1}};
    for (const Pomodoro& p : db.pomodorosBetween(day, day))
        claim(free, p.startedAt, qMax(p.endedAt, p.startedAt + p.studiedSec));
    for (const Interval& i : db.intervalsBetween(day, day))
        claim(free, i.startedAt, i.startedAt + i.seconds);
    for (const Span& s : free) out.total += s.second - s.first;
    for (const Occurrence& o : occurrences(evs, day)) {
        const qint64 got = claim(free, o.start, qMin(o.end, w1));
        if (got > 0) out.byCategory[o.ev->category.isEmpty() ? o.ev->title : o.ev->category] += got;
    }
    for (const Span& s : free) out.unexplained += s.second - s.first;
    return out;
}

} // namespace routine
