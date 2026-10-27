#include "core/Database.h"
#include "core/Format.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
constexpr int kSchemaVersion = 4;

const char* kSchema[] = {
    R"(CREATE TABLE IF NOT EXISTS folders (
        id         INTEGER PRIMARY KEY AUTOINCREMENT,
        parent_id  INTEGER REFERENCES folders(id) ON DELETE CASCADE,
        name       TEXT NOT NULL,
        sort       INTEGER NOT NULL DEFAULT 0,
        created_at INTEGER NOT NULL
    ))",
    R"(CREATE TABLE IF NOT EXISTS days (
        day   TEXT PRIMARY KEY,
        title TEXT NOT NULL DEFAULT ''
    ))",
    R"(CREATE TABLE IF NOT EXISTS pomodoros (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        folder_id   INTEGER REFERENCES folders(id) ON DELETE SET NULL,
        day         TEXT NOT NULL,
        started_at  INTEGER NOT NULL,
        ended_at    INTEGER NOT NULL,
        planned_sec INTEGER NOT NULL,
        studied_sec INTEGER NOT NULL DEFAULT 0,
        status      INTEGER NOT NULL
    ))",
    "CREATE INDEX IF NOT EXISTS idx_pomodoros_day ON pomodoros(day)",
    R"(CREATE TABLE IF NOT EXISTS intervals (
        id         INTEGER PRIMARY KEY AUTOINCREMENT,
        kind       INTEGER NOT NULL,
        day        TEXT NOT NULL,
        started_at INTEGER NOT NULL,
        seconds    INTEGER NOT NULL
    ))",
    "CREATE INDEX IF NOT EXISTS idx_intervals_day ON intervals(day)",
    R"(CREATE TABLE IF NOT EXISTS milestones (
        folder_id INTEGER NOT NULL,
        hours     INTEGER NOT NULL,
        reached_at INTEGER NOT NULL,
        PRIMARY KEY (folder_id, hours)
    ))",
    // M7: metas diarias (ver core/Goals.h)
    R"(CREATE TABLE IF NOT EXISTS goals (
        id         INTEGER PRIMARY KEY AUTOINCREMENT,
        title      TEXT NOT NULL DEFAULT '',
        spec       TEXT NOT NULL DEFAULT '',
        start_day  TEXT NOT NULL,
        end_day    TEXT,
        weekdays   INTEGER NOT NULL DEFAULT 0,
        created_at INTEGER NOT NULL
    ))",
    R"(CREATE TABLE IF NOT EXISTS settings (
        key   TEXT PRIMARY KEY,
        value TEXT
    ))",
};
} // namespace

Database::Database()
    : m_conn(QStringLiteral("constancia-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

Database::~Database()
{
    if (m_db.isValid()) {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_conn);
    }
}

bool Database::open(const QString& path)
{
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_conn);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        m_error = m_db.lastError().text();
        return false;
    }
    exec("PRAGMA foreign_keys = ON");
    exec("PRAGMA journal_mode = WAL");
    exec("PRAGMA synchronous = NORMAL");
    return migrate();
}

bool Database::exec(const QString& sql)
{
    QSqlQuery q(m_db);
    if (!q.exec(sql)) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::migrate()
{
    for (const char* stmt : kSchema)
        if (!exec(QString::fromUtf8(stmt))) return false;

    int version = 0;
    {
        QSqlQuery q(m_db);
        q.exec("PRAGMA user_version");
        if (q.next()) version = q.value(0).toInt();
    }
    if (version < 1) {
        // Primera vez: una actividad para arrancar sin fricción.
        QSqlQuery c(m_db);
        c.exec("SELECT COUNT(*) FROM folders");
        const bool empty = c.next() && c.value(0).toInt() == 0;
        c.finish();
        if (empty) addFolder(0, QStringLiteral("General"));
    }
    // Cada módulo agrega sus columnas solo si faltan: así sacar un módulo no
    // rompe la base (la columna queda ahí, sin usar).
    if (version < 2) addColumnIfMissing("pomodoros", "linked", "INTEGER NOT NULL DEFAULT 0");  // encadenados
    if (version < 3) addColumnIfMissing("pomodoros", "note", "TEXT NOT NULL DEFAULT ''");      // M6: nota
    if (version < kSchemaVersion) exec(QString("PRAGMA user_version = %1").arg(kSchemaVersion));
    return true;
}

void Database::addColumnIfMissing(const QString& table, const QString& column, const QString& decl)
{
    QSqlQuery info(m_db);
    info.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table));
    bool has = false;
    while (info.next()) has |= info.value(1).toString() == column;
    info.finish();
    if (!has) exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, decl));
}

// ---------------------------------------------------------------------------
// Carpetas
// ---------------------------------------------------------------------------
QVector<Folder> Database::folders() const
{
    QVector<Folder> out;
    QSqlQuery q(m_db);
    q.exec("SELECT id, COALESCE(parent_id, 0), name, sort FROM folders");
    while (q.next())
        out.append({q.value(0).toLongLong(), q.value(1).toLongLong(), q.value(2).toString(), q.value(3).toInt()});
    return out;
}

qint64 Database::addFolder(qint64 parentId, const QString& name)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO folders(parent_id, name, sort, created_at) VALUES(?, ?, "
              "(SELECT COALESCE(MAX(sort), 0) + 1 FROM folders), ?)");
    q.addBindValue(parentId > 0 ? QVariant(parentId) : QVariant(QMetaType(QMetaType::LongLong)));
    q.addBindValue(name.trimmed());
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!q.exec()) {
        m_error = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

bool Database::renameFolder(qint64 id, const QString& name)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE folders SET name = ? WHERE id = ?");
    q.addBindValue(name.trimmed());
    q.addBindValue(id);
    return q.exec();
}

bool Database::deleteFolder(qint64 id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM folders WHERE id = ?");
    q.addBindValue(id);
    return q.exec();
}

bool Database::moveFolder(qint64 id, qint64 newParentId)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE folders SET parent_id = ? WHERE id = ?");
    q.addBindValue(newParentId > 0 ? QVariant(newParentId) : QVariant(QMetaType(QMetaType::LongLong)));
    q.addBindValue(id);
    return q.exec();
}

void Database::setFolderPlacement(qint64 id, qint64 parentId, int sort)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE folders SET parent_id = ?, sort = ? WHERE id = ?");
    q.addBindValue(parentId > 0 ? QVariant(parentId) : QVariant(QMetaType(QMetaType::LongLong)));
    q.addBindValue(sort);
    q.addBindValue(id);
    q.exec();
}

// ---------------------------------------------------------------------------
// Días
// ---------------------------------------------------------------------------
QString Database::dayTitle(const QDate& day) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT title FROM days WHERE day = ?");
    q.addBindValue(fmt::dayKey(day));
    q.exec();
    return q.next() ? q.value(0).toString() : QString();
}

QHash<QDate, QString> Database::dayTitles(const QDate& from, const QDate& to) const
{
    QHash<QDate, QString> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT day, title FROM days WHERE day BETWEEN ? AND ? AND title <> ''");
    q.addBindValue(fmt::dayKey(from));
    q.addBindValue(fmt::dayKey(to));
    q.exec();
    while (q.next())
        out.insert(fmt::fromDayKey(q.value(0).toString()), q.value(1).toString());
    return out;
}

void Database::setDayTitle(const QDate& day, const QString& title)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO days(day, title) VALUES(?, ?) "
              "ON CONFLICT(day) DO UPDATE SET title = excluded.title");
    q.addBindValue(fmt::dayKey(day));
    q.addBindValue(title.trimmed());
    q.exec();
}

// ---------------------------------------------------------------------------
// Pomodoros
// ---------------------------------------------------------------------------
Pomodoro Database::readPomodoro(const QSqlQuery& q)
{
    Pomodoro p;
    p.id         = q.value(0).toLongLong();
    p.folderId   = q.value(1).toLongLong();
    p.day        = fmt::fromDayKey(q.value(2).toString());
    p.startedAt  = q.value(3).toLongLong();
    p.endedAt    = q.value(4).toLongLong();
    p.plannedSec = q.value(5).toInt();
    p.studiedSec = q.value(6).toInt();
    p.status     = PomoStatus(q.value(7).toInt());
    p.linked     = q.value(8).toInt() != 0;
    p.note       = q.value(9).toString();
    return p;
}

#define POMO_COLUMNS "id, COALESCE(folder_id, 0), day, started_at, ended_at, planned_sec, studied_sec, status, linked, note"

qint64 Database::insertPomodoro(const Pomodoro& p)
{
    QSqlQuery q(m_db);
    // Con id > 0 se reinserta con el mismo id (deshacer un borrado).
    q.prepare(QStringLiteral("INSERT INTO pomodoros(%1folder_id, day, started_at, ended_at, planned_sec, studied_sec, status, linked, note) "
                             "VALUES(%2?, ?, ?, ?, ?, ?, ?, ?, ?)")
                  .arg(p.id > 0 ? QStringLiteral("id, ") : QString(), p.id > 0 ? QStringLiteral("?, ") : QString()));
    if (p.id > 0) q.addBindValue(p.id);
    q.addBindValue(p.folderId > 0 ? QVariant(p.folderId) : QVariant(QMetaType(QMetaType::LongLong)));
    q.addBindValue(fmt::dayKey(p.day));
    q.addBindValue(p.startedAt);
    q.addBindValue(p.endedAt);
    q.addBindValue(p.plannedSec);
    q.addBindValue(p.studiedSec);
    q.addBindValue(int(p.status));
    q.addBindValue(p.linked ? 1 : 0);
    q.addBindValue(p.note.isNull() ? QStringLiteral("") : p.note);   // NULL no entra en NOT NULL
    if (!q.exec()) {
        m_error = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

void Database::updatePomodoro(const Pomodoro& p)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE pomodoros SET folder_id = ?, ended_at = ?, planned_sec = ?, studied_sec = ?, status = ? "
              "WHERE id = ?");
    q.addBindValue(p.folderId > 0 ? QVariant(p.folderId) : QVariant(QMetaType(QMetaType::LongLong)));
    q.addBindValue(p.endedAt);
    q.addBindValue(p.plannedSec);
    q.addBindValue(p.studiedSec);
    q.addBindValue(int(p.status));
    q.addBindValue(p.id);
    q.exec();
}

void Database::setPomodoroFolder(qint64 id, qint64 folderId)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE pomodoros SET folder_id = ? WHERE id = ?");
    q.addBindValue(folderId > 0 ? QVariant(folderId) : QVariant(QMetaType(QMetaType::LongLong)));
    q.addBindValue(id);
    q.exec();
}

void Database::setPomodoroNote(qint64 id, const QString& note)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE pomodoros SET note = ? WHERE id = ?");
    q.addBindValue(note.isNull() ? QStringLiteral("") : note.trimmed());
    q.addBindValue(id);
    q.exec();
}

void Database::setPomodoroDuration(qint64 id, int studiedSec)
{
    // Corregir cuánto duró uno terminado: si fue más largo que lo pactado, se
    // agranda también lo pactado (así se ve completo).
    QSqlQuery q(m_db);
    q.prepare("UPDATE pomodoros SET studied_sec = ?, planned_sec = MAX(planned_sec, ?), ended_at = started_at + ? "
              "WHERE id = ?");
    q.addBindValue(studiedSec);
    q.addBindValue(studiedSec);
    q.addBindValue(studiedSec);
    q.addBindValue(id);
    q.exec();
}

void Database::deletePomodoro(qint64 id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM pomodoros WHERE id = ?");
    q.addBindValue(id);
    q.exec();
}

std::optional<Pomodoro> Database::pomodoro(qint64 id) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT " POMO_COLUMNS " FROM pomodoros WHERE id = ?");
    q.addBindValue(id);
    q.exec();
    if (!q.next()) return std::nullopt;
    return readPomodoro(q);
}

QVector<Pomodoro> Database::pomodorosBetween(const QDate& from, const QDate& to) const
{
    QVector<Pomodoro> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT " POMO_COLUMNS " FROM pomodoros WHERE day BETWEEN ? AND ? ORDER BY started_at, id");
    q.addBindValue(fmt::dayKey(from));
    q.addBindValue(fmt::dayKey(to));
    q.exec();
    while (q.next()) out.append(readPomodoro(q));
    return out;
}

QVector<Pomodoro> Database::pomodorosWithStatus(PomoStatus st) const
{
    QVector<Pomodoro> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT " POMO_COLUMNS " FROM pomodoros WHERE status = ? ORDER BY started_at, id");
    q.addBindValue(int(st));
    q.exec();
    while (q.next()) out.append(readPomodoro(q));
    return out;
}

QSet<QDate> Database::studiedDays() const
{
    QSet<QDate> out;
    QSqlQuery q(m_db);
    q.exec("SELECT DISTINCT day FROM pomodoros");
    while (q.next()) out.insert(fmt::fromDayKey(q.value(0).toString()));
    return out;
}

QHash<qint64, qint64> Database::studyByFolder(const QDate& from, const QDate& to) const
{
    QHash<qint64, qint64> out;
    QSqlQuery q(m_db);
    if (from.isValid() && to.isValid()) {
        q.prepare("SELECT COALESCE(folder_id, 0), SUM(studied_sec) FROM pomodoros "
                  "WHERE day BETWEEN ? AND ? GROUP BY folder_id");
        q.addBindValue(fmt::dayKey(from));
        q.addBindValue(fmt::dayKey(to));
        q.exec();
    } else {
        q.exec("SELECT COALESCE(folder_id, 0), SUM(studied_sec) FROM pomodoros GROUP BY folder_id");
    }
    while (q.next()) out.insert(q.value(0).toLongLong(), q.value(1).toLongLong());
    return out;
}

// ---------------------------------------------------------------------------
// Intervalos
// ---------------------------------------------------------------------------
qint64 Database::insertInterval(IntervalKind kind, const QDate& day, qint64 startedAt, qint64 seconds)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO intervals(kind, day, started_at, seconds) VALUES(?, ?, ?, ?)");
    q.addBindValue(int(kind));
    q.addBindValue(fmt::dayKey(day));
    q.addBindValue(startedAt);
    q.addBindValue(seconds);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

void Database::updateIntervalSeconds(qint64 id, qint64 seconds)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE intervals SET seconds = ? WHERE id = ?");
    q.addBindValue(seconds);
    q.addBindValue(id);
    q.exec();
}

void Database::setIntervalsKind(const QVector<qint64>& ids, IntervalKind kind)
{
    if (ids.isEmpty()) return;
    m_db.transaction();
    QSqlQuery q(m_db);
    q.prepare("UPDATE intervals SET kind = ? WHERE id = ?");
    for (qint64 id : ids) {
        q.addBindValue(int(kind));
        q.addBindValue(id);
        q.exec();
    }
    m_db.commit();
}

QVector<Interval> Database::intervalsBetween(const QDate& from, const QDate& to) const
{
    QVector<Interval> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, kind, day, started_at, seconds FROM intervals WHERE day BETWEEN ? AND ?");
    q.addBindValue(fmt::dayKey(from));
    q.addBindValue(fmt::dayKey(to));
    q.exec();
    while (q.next()) {
        Interval i;
        i.id        = q.value(0).toLongLong();
        i.kind      = IntervalKind(q.value(1).toInt());
        i.day       = fmt::fromDayKey(q.value(2).toString());
        i.startedAt = q.value(3).toLongLong();
        i.seconds   = q.value(4).toLongLong();
        out.append(i);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Hitos y ajustes
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Metas (M7)
// ---------------------------------------------------------------------------
QVector<GoalRule> Database::goals() const
{
    QVector<GoalRule> out;
    QSqlQuery q(m_db);
    q.exec("SELECT id, title, spec, start_day, COALESCE(end_day, ''), weekdays FROM goals ORDER BY id");
    while (q.next()) {
        GoalRule r;
        r.id = q.value(0).toLongLong();
        r.title = q.value(1).toString();
        r.spec = q.value(2).toString();
        r.start = fmt::fromDayKey(q.value(3).toString());
        const QString end = q.value(4).toString();
        if (!end.isEmpty()) r.end = fmt::fromDayKey(end);
        r.weekdays = q.value(5).toInt();
        out.append(r);
    }
    return out;
}

static QVariant nonNull(const QString& s) { return s.isNull() ? QStringLiteral("") : s; }

qint64 Database::insertGoal(const GoalRule& r)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO goals(title, spec, start_day, end_day, weekdays, created_at) VALUES(?, ?, ?, ?, ?, ?)");
    q.addBindValue(nonNull(r.title));
    q.addBindValue(nonNull(r.spec));
    q.addBindValue(fmt::dayKey(r.start));
    q.addBindValue(r.end.isValid() ? QVariant(fmt::dayKey(r.end)) : QVariant(QMetaType(QMetaType::QString)));
    q.addBindValue(r.weekdays);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!q.exec()) { m_error = q.lastError().text(); return 0; }
    return q.lastInsertId().toLongLong();
}

void Database::updateGoal(const GoalRule& r)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE goals SET title = ?, spec = ?, start_day = ?, end_day = ?, weekdays = ? WHERE id = ?");
    q.addBindValue(nonNull(r.title));
    q.addBindValue(nonNull(r.spec));
    q.addBindValue(fmt::dayKey(r.start));
    q.addBindValue(r.end.isValid() ? QVariant(fmt::dayKey(r.end)) : QVariant(QMetaType(QMetaType::QString)));
    q.addBindValue(r.weekdays);
    q.addBindValue(r.id);
    q.exec();
}

void Database::deleteGoal(qint64 id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM goals WHERE id = ?");
    q.addBindValue(id);
    q.exec();
}

bool Database::backupTo(const QString& path)
{
    QSqlQuery q(m_db);
    q.prepare("VACUUM INTO ?");
    q.addBindValue(path);
    if (!q.exec()) { m_error = q.lastError().text(); return false; }
    return true;
}

bool Database::markMilestone(qint64 folderId, int hours)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO milestones(folder_id, hours, reached_at) VALUES(?, ?, ?)");
    q.addBindValue(folderId);
    q.addBindValue(hours);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    return q.exec() && q.numRowsAffected() > 0;
}

QVariant Database::setting(const QString& key, const QVariant& def) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM settings WHERE key = ?");
    q.addBindValue(key);
    q.exec();
    return q.next() ? q.value(0) : def;
}

void Database::setSetting(const QString& key, const QVariant& value)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO settings(key, value) VALUES(?, ?) "
              "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    q.addBindValue(key);
    q.addBindValue(value.toString());
    q.exec();
}
