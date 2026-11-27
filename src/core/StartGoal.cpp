#include "core/StartGoal.h"

#include "core/DayClock.h"
#include "core/FolderTree.h"

#include <QSqlQuery>

namespace startgoal {

static Goal fromQuery(const QSqlQuery& q)
{
    Goal g;
    g.day = QDate::fromString(q.value(0).toString(), Qt::ISODate);
    g.minute = q.value(1).toInt();
    g.folderId = q.value(2).toLongLong();
    g.pomos = qMax(1, q.value(3).toInt());
    return g;
}

std::optional<Goal> get(const Database& db, const QDate& day)
{
    QSqlQuery q(db.handle());
    q.prepare("SELECT day, minute, folder_id, pomos FROM start_goals WHERE day = ?");
    q.addBindValue(day.toString(Qt::ISODate));
    q.exec();
    if (q.next()) return fromQuery(q);
    return std::nullopt;
}

void save(Database& db, const Goal& g)
{
    QSqlQuery q(db.handle());
    q.prepare("INSERT OR REPLACE INTO start_goals(day, minute, folder_id, pomos) VALUES(?, ?, ?, ?)");
    q.addBindValue(g.day.toString(Qt::ISODate));
    q.addBindValue(qBound(0, g.minute, 24 * 60 - 1));
    q.addBindValue(g.folderId);
    q.addBindValue(qMax(1, g.pomos));
    q.exec();
}

void remove(Database& db, const QDate& day)
{
    QSqlQuery q(db.handle());
    q.prepare("DELETE FROM start_goals WHERE day = ?");
    q.addBindValue(day.toString(Qt::ISODate));
    q.exec();
}

QVector<Goal> between(const Database& db, const QDate& from, const QDate& to)
{
    QVector<Goal> out;
    QSqlQuery q(db.handle());
    q.prepare("SELECT day, minute, folder_id, pomos FROM start_goals WHERE day BETWEEN ? AND ? ORDER BY day");
    q.addBindValue(from.toString(Qt::ISODate));
    q.addBindValue(to.toString(Qt::ISODate));
    q.exec();
    while (q.next()) out.append(fromQuery(q));
    return out;
}

QDateTime when(const Goal& g)
{
    // Con el día lógico empezando a las 4 AM, «02:00» de un día es de madrugada del calendario siguiente.
    QDate d = g.day;
    if (g.minute < dayclock::startHour() * 60) d = d.addDays(1);
    return QDateTime(d, QTime(g.minute / 60, g.minute % 60));
}

Result evaluate(const Database& db, const Goal& g, const QDateTime& now)
{
    Result r;
    const QDateTime goalAt = when(g);
    const QSet<qint64> sub = g.folderId > 0 ? FolderTree(db.folders()).subtree(g.folderId) : QSet<qint64>();
    qint64 first = 0;
    for (const Pomodoro& p : db.pomodorosBetween(g.day, g.day)) {
        if (p.studiedSec <= 0 || p.manual) continue;
        if (!first || p.startedAt < first) first = p.startedAt;
        if (g.folderId <= 0 || sub.contains(p.folderId)) r.done += p.doneUnits();
    }
    r.taskDone = r.done >= g.pomos;
    if (first) {
        r.started = true;
        r.firstStart = QDateTime::fromSecsSinceEpoch(first);
        const qint64 late = goalAt.secsTo(r.firstStart) / 60;
        r.lateMin = int(qMax<qint64>(0, late));
        r.onTime = late <= kToleranceMin;
    }
    r.met = r.started && r.onTime && r.taskDone;
    // Pendiente: hoy y todavía puede cumplirse (no pasó la hora + tolerancia, o arrancó a tiempo y falta la tarea).
    const bool dayOpen = dayclock::logicalDate(now) == g.day;
    if (dayOpen && !r.met) r.pending = r.started ? r.onTime : now < goalAt.addSecs(kToleranceMin * 60);
    return r;
}

} // namespace startgoal
