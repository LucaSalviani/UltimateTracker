#include "core/AppUsage.h"

#include <QSqlQuery>

namespace appusage {

void add(Database& db, const QDate& day, const QString& exe, const QString& title, bool inPomodoro, qint64 sec)
{
    if (sec <= 0 || exe.isEmpty()) return;
    QSqlQuery q(db.handle());
    q.prepare("INSERT INTO app_usage(day, exe, title, in_pomo, sec) VALUES(?, ?, ?, ?, ?) "
              "ON CONFLICT(day, exe, title, in_pomo) DO UPDATE SET sec = sec + excluded.sec");
    q.addBindValue(day.toString(Qt::ISODate));
    q.addBindValue(exe.left(80));
    q.addBindValue(title.left(120));
    q.addBindValue(inPomodoro ? 1 : 0);
    q.addBindValue(sec);
    q.exec();
}

QVector<Row> byApp(const Database& db, const QDate& from, const QDate& to)
{
    QVector<Row> out;
    QSqlQuery q(db.handle());
    q.prepare("SELECT exe, SUM(CASE WHEN in_pomo = 1 THEN sec ELSE 0 END), SUM(CASE WHEN in_pomo = 0 THEN sec ELSE 0 END) "
              "FROM app_usage WHERE day BETWEEN ? AND ? GROUP BY exe ORDER BY SUM(sec) DESC");
    q.addBindValue(from.toString(Qt::ISODate));
    q.addBindValue(to.toString(Qt::ISODate));
    q.exec();
    while (q.next()) out.append({q.value(0).toString(), q.value(1).toLongLong(), q.value(2).toLongLong()});
    return out;
}

QVector<QPair<QString, qint64>> titles(const Database& db, const QDate& from, const QDate& to, const QString& exe, int limit)
{
    QVector<QPair<QString, qint64>> out;
    QSqlQuery q(db.handle());
    q.prepare("SELECT title, SUM(sec) FROM app_usage WHERE day BETWEEN ? AND ? AND exe = ? AND title <> '' "
              "GROUP BY title ORDER BY SUM(sec) DESC LIMIT ?");
    q.addBindValue(from.toString(Qt::ISODate));
    q.addBindValue(to.toString(Qt::ISODate));
    q.addBindValue(exe);
    q.addBindValue(limit);
    q.exec();
    while (q.next()) out.append({q.value(0).toString(), q.value(1).toLongLong()});
    return out;
}

bool any(const Database& db)
{
    QSqlQuery q(db.handle());
    q.exec("SELECT 1 FROM app_usage LIMIT 1");
    return q.next();
}

void clear(Database& db)
{
    QSqlQuery q(db.handle());
    q.exec("DELETE FROM app_usage");
}

} // namespace appusage
