#include "core/Notebook.h"

#include <QDateTime>
#include <QSqlQuery>

namespace notebook {

static Page fromQuery(const QSqlQuery& q)
{
    Page p;
    p.id = q.value(0).toLongLong();
    p.title = q.value(1).toString();
    p.body = q.value(2).toString();
    p.pinned = q.value(3).toInt() != 0;
    p.createdAt = q.value(4).toLongLong();
    p.updatedAt = q.value(5).toLongLong();
    return p;
}

QVector<Page> pages(const Database& db, const QString& query)
{
    QVector<Page> out;
    QSqlQuery q(db.handle());
    if (query.trimmed().isEmpty()) {
        q.exec("SELECT id, title, body, pinned, created_at, updated_at FROM notebook_pages ORDER BY pinned DESC, updated_at DESC, id DESC");
    } else {
        q.prepare("SELECT id, title, body, pinned, created_at, updated_at FROM notebook_pages "
                  "WHERE title LIKE ? OR body LIKE ? ORDER BY pinned DESC, updated_at DESC, id DESC");
        const QString like = QStringLiteral("%") + query.trimmed() + QStringLiteral("%");
        q.addBindValue(like);
        q.addBindValue(like);
        q.exec();
    }
    while (q.next()) out.append(fromQuery(q));
    return out;
}

Page get(const Database& db, qint64 id)
{
    QSqlQuery q(db.handle());
    q.prepare("SELECT id, title, body, pinned, created_at, updated_at FROM notebook_pages WHERE id = ?");
    q.addBindValue(id);
    q.exec();
    return q.next() ? fromQuery(q) : Page{};
}

qint64 save(Database& db, const Page& p)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery q(db.handle());
    if (p.id) {
        q.prepare("UPDATE notebook_pages SET title=?, body=?, pinned=?, updated_at=? WHERE id=?");
        q.addBindValue(p.title);
        q.addBindValue(p.body);
        q.addBindValue(p.pinned ? 1 : 0);
        q.addBindValue(now);
        q.addBindValue(p.id);
        q.exec();
        return p.id;
    }
    q.prepare("INSERT INTO notebook_pages(title, body, pinned, created_at, updated_at) VALUES(?,?,?,?,?)");
    q.addBindValue(p.title);
    q.addBindValue(p.body);
    q.addBindValue(p.pinned ? 1 : 0);
    q.addBindValue(now);
    q.addBindValue(now);
    q.exec();
    return q.lastInsertId().toLongLong();
}

void remove(Database& db, qint64 id)
{
    QSqlQuery q(db.handle());
    q.prepare("DELETE FROM notebook_pages WHERE id = ?");
    q.addBindValue(id);
    q.exec();
}

QString exportText(const QVector<Page>& pages)
{
    QStringList parts;
    for (const Page& p : pages)
        parts << QStringLiteral("# %1\n(%2)\n\n%3\n").arg(p.title.isEmpty() ? QStringLiteral("Sin título") : p.title,
                                                         QDateTime::fromSecsSinceEpoch(p.updatedAt).toString(QStringLiteral("dd/MM/yyyy HH:mm")), p.body);
    return parts.join(QStringLiteral("\n---\n\n"));
}

} // namespace notebook
