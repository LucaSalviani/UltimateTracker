#include "core/Sync.h"
#include "core/Database.h"
#include "core/SyncConfig.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QUrlQuery>
#include <QUuid>
#include <QVariant>
#include <algorithm>

// ===========================================================================
// SyncStore
// ===========================================================================
namespace {
const char* tableFor(const QString& kind)
{
    if (kind == QLatin1String("folder")) return "folders";
    if (kind == QLatin1String("pomodoro")) return "pomodoros";
    if (kind == QLatin1String("interval")) return "intervals";
    if (kind == QLatin1String("goal")) return "goals";
    if (kind == QLatin1String("day")) return "days";
    return nullptr;
}
QVariant nullable(const QJsonValue& v) { return v.isNull() || v.isUndefined() ? QVariant() : v.toVariant(); }
} // namespace

SyncStore::SyncStore(Database& db) : m_db(db) {}

QString SyncStore::deviceId()
{
    QString id = m_db.setting("sync/device").toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_db.setSetting("sync/device", id);
    }
    return id;
}

QString SyncStore::uidOf(const char* table, qint64 id)
{
    if (id <= 0) return {};
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral("SELECT uid FROM %1 WHERE id = ?").arg(QLatin1String(table)));
    q.addBindValue(id);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}

qint64 SyncStore::idOf(const char* table, const QString& uid)
{
    if (uid.isEmpty()) return 0;
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral("SELECT id FROM %1 WHERE uid = ?").arg(QLatin1String(table)));
    q.addBindValue(uid);
    return q.exec() && q.next() ? q.value(0).toLongLong() : 0;
}

int SyncStore::pendingCount()
{
    QSqlQuery q(m_db.handle());
    q.exec("SELECT COUNT(*) FROM sync_outbox");
    return q.next() ? q.value(0).toInt() : 0;
}

QVector<SyncRecord> SyncStore::pending(int limit)
{
    QVector<SyncRecord> out;
    struct Item { QString kind, uid; bool deleted; qint64 at; };
    QVector<Item> items;
    {
        QSqlQuery q(m_db.handle());
        q.prepare("SELECT kind, uid, deleted, at FROM sync_outbox ORDER BY at LIMIT ?");
        q.addBindValue(limit);
        q.exec();
        while (q.next()) items.append({q.value(0).toString(), q.value(1).toString(), q.value(2).toBool(), q.value(3).toLongLong()});
    }
    const QString device = deviceId();
    static const QRegularExpression atId(QStringLiteral("@(\\d+)"));
    for (const Item& it : items) {
        SyncRecord r;
        r.kind = it.kind;
        r.uid = it.uid;
        r.updatedAt = it.at;
        r.deleted = it.deleted;
        if (!it.deleted) {
            QSqlQuery q(m_db.handle());
            if (it.kind == QLatin1String("folder")) {
                q.prepare("SELECT name, COALESCE(parent_id, 0), sort, prior_sec, color, created_at, updated_at FROM folders WHERE uid = ?");
                q.addBindValue(it.uid);
                if (!q.exec() || !q.next()) { r.deleted = true; out.append(r); continue; }
                const QString parent = uidOf("folders", q.value(1).toLongLong());
                r.data = {{"name", q.value(0).toString()}, {"parent", parent.isEmpty() ? QJsonValue() : QJsonValue(parent)},
                          {"sort", q.value(2).toInt()}, {"prior_sec", q.value(3).toLongLong()}, {"color", q.value(4).toString()},
                          {"created_at", q.value(5).toLongLong()}};
                r.updatedAt = q.value(6).toLongLong();
            } else if (it.kind == QLatin1String("pomodoro")) {
                q.prepare("SELECT COALESCE(folder_id, 0), day, started_at, ended_at, planned_sec, studied_sec, status, linked, note, device, updated_at "
                          "FROM pomodoros WHERE uid = ?");
                q.addBindValue(it.uid);
                if (!q.exec() || !q.next()) { r.deleted = true; out.append(r); continue; }
                const QString folder = uidOf("folders", q.value(0).toLongLong());
                r.data = {{"folder", folder.isEmpty() ? QJsonValue() : QJsonValue(folder)}, {"day", q.value(1).toString()},
                          {"started_at", q.value(2).toLongLong()}, {"ended_at", q.value(3).toLongLong()},
                          {"planned_sec", q.value(4).toInt()}, {"studied_sec", q.value(5).toInt()}, {"status", q.value(6).toInt()},
                          {"linked", q.value(7).toBool()}, {"note", q.value(8).toString()},
                          {"device", q.value(9).toString().isEmpty() ? device : q.value(9).toString()}};
                r.updatedAt = q.value(10).toLongLong();
            } else if (it.kind == QLatin1String("interval")) {
                q.prepare("SELECT kind, day, started_at, seconds, updated_at FROM intervals WHERE uid = ?");
                q.addBindValue(it.uid);
                if (!q.exec() || !q.next()) { r.deleted = true; out.append(r); continue; }
                r.data = {{"kind", q.value(0).toInt()}, {"day", q.value(1).toString()}, {"started_at", q.value(2).toLongLong()},
                          {"seconds", q.value(3).toLongLong()}};
                r.updatedAt = q.value(4).toLongLong();
            } else if (it.kind == QLatin1String("goal")) {
                q.prepare("SELECT title, spec, start_day, end_day, weekdays, created_at, updated_at FROM goals WHERE uid = ?");
                q.addBindValue(it.uid);
                if (!q.exec() || !q.next()) { r.deleted = true; out.append(r); continue; }
                // "3x30@7" → "3x30@<uid de la actividad 7>": los ids locales no sirven en el otro aparato.
                QString spec = q.value(1).toString();
                QString outSpec;
                qsizetype last = 0;
                for (auto m = atId.globalMatch(spec); m.hasNext();) {
                    const auto mm = m.next();
                    outSpec += spec.mid(last, mm.capturedStart() - last);
                    const QString u = uidOf("folders", mm.captured(1).toLongLong());
                    if (!u.isEmpty()) outSpec += QLatin1Char('@') + u;
                    last = mm.capturedEnd();
                }
                outSpec += spec.mid(last);
                r.data = {{"title", q.value(0).toString()}, {"spec", outSpec}, {"start_day", q.value(2).toString()},
                          {"end_day", q.value(3).isNull() ? QJsonValue() : QJsonValue(q.value(3).toString())},
                          {"weekdays", q.value(4).toInt()}, {"created_at", q.value(5).toLongLong()}};
                r.updatedAt = q.value(6).toLongLong();
            } else if (it.kind == QLatin1String("day")) {
                q.prepare("SELECT title, updated_at FROM days WHERE day = ?");
                q.addBindValue(it.uid);
                if (!q.exec() || !q.next()) { r.deleted = true; out.append(r); continue; }
                r.data = {{"title", q.value(0).toString()}};
                r.updatedAt = q.value(1).toLongLong();
            }
        }
        out.append(r);
    }
    return out;
}

void SyncStore::acknowledge(const QVector<SyncRecord>& sent)
{
    QSqlDatabase& h = m_db.handle();
    h.transaction();
    for (const SyncRecord& r : sent) {
        // Si cambió de nuevo mientras se mandaba (otra hora), queda anotado para la próxima.
        QSqlQuery q(h);
        q.prepare("DELETE FROM sync_outbox WHERE kind = ? AND uid = ? AND at <= ?");
        q.addBindValue(r.kind);
        q.addBindValue(r.uid);
        q.addBindValue(r.updatedAt);
        q.exec();
    }
    h.commit();
}

int SyncStore::apply(const QVector<SyncRecord>& incoming)
{
    // Orden: actividades (padres antes que hijas), después el resto; los borrados de actividades al final.
    QVector<SyncRecord> folders, others, folderDeletes;
    for (const SyncRecord& r : incoming) {
        if (!tableFor(r.kind)) continue;
        if (r.kind == QLatin1String("folder")) (r.deleted ? folderDeletes : folders).append(r);
        else others.append(r);
    }
    QVector<SyncRecord> ordered;
    QSet<QString> pendingUids;
    for (const SyncRecord& f : folders) pendingUids.insert(f.uid);
    while (!folders.isEmpty()) {
        bool progress = false;
        for (int i = 0; i < folders.size();) {
            const QString parent = folders[i].data.value("parent").toString();
            if (parent.isEmpty() || !pendingUids.contains(parent)) {
                pendingUids.remove(folders[i].uid);
                ordered.append(folders.takeAt(i));
                progress = true;
            } else {
                ++i;
            }
        }
        if (!progress) { ordered += folders; break; }   // ciclo raro: que entren igual
    }
    ordered += others;
    ordered += folderDeletes;

    QSqlDatabase& h = m_db.handle();
    h.transaction();
    QSqlQuery(h).exec("UPDATE sync_flag SET applying = 1");
    int changed = 0;
    for (const SyncRecord& r : ordered)
        if (applyOne(r)) ++changed;
    QSqlQuery(h).exec("UPDATE sync_flag SET applying = 0");
    h.commit();
    return changed;
}

bool SyncStore::applyOne(const SyncRecord& r)
{
    QSqlDatabase& h = m_db.handle();
    const char* table = tableFor(r.kind);
    const bool isDay = r.kind == QLatin1String("day");
    const QString keyCol = isDay ? QStringLiteral("day") : QStringLiteral("uid");

    // ¿Lo local es más nuevo? Entonces se queda lo local (y se va a mandar).
    qint64 localAt = -1;
    qint64 localId = 0;
    {
        QSqlQuery q(h);
        q.prepare(QStringLiteral("SELECT %1, updated_at FROM %2 WHERE %3 = ?").arg(isDay ? QStringLiteral("0") : QStringLiteral("id"),
                                                                                  QLatin1String(table), keyCol));
        q.addBindValue(r.uid);
        if (q.exec() && q.next()) { localId = q.value(0).toLongLong(); localAt = q.value(1).toLongLong(); }
    }
    if (localAt > r.updatedAt) return false;
    if (localAt == r.updatedAt && !r.deleted) {
        // Es lo mismo que ya teníamos (nuestro propio cambio que vuelve).
        return false;
    }
    auto dropOutbox = [&] {
        QSqlQuery q(h);
        q.prepare("DELETE FROM sync_outbox WHERE kind = ? AND uid = ? AND at <= ?");
        q.addBindValue(r.kind);
        q.addBindValue(r.uid);
        q.addBindValue(r.updatedAt);
        q.exec();
    };

    if (r.deleted) {
        if (localAt < 0) return false;
        QSqlQuery q(h);
        q.prepare(QStringLiteral("DELETE FROM %1 WHERE %2 = ?").arg(QLatin1String(table), keyCol));
        q.addBindValue(r.uid);
        q.exec();
        dropOutbox();
        return true;
    }

    const QJsonObject& d = r.data;
    QSqlQuery q(h);
    if (r.kind == QLatin1String("folder")) {
        const qint64 parent = idOf("folders", d.value("parent").toString());
        if (localAt < 0) {
            // Primera vez que llega: si ya hay una actividad igual (mismo nombre y lugar),
            // es la misma (la "General" de cada aparato): se queda con el id global de esta.
            QSqlQuery same(h);
            same.prepare("SELECT id, uid FROM folders WHERE name = ? AND COALESCE(parent_id, 0) = ? LIMIT 1");
            same.addBindValue(d.value("name").toString());
            same.addBindValue(parent);
            if (same.exec() && same.next()) {
                localId = same.value(0).toLongLong();
                const QString oldUid = same.value(1).toString();
                QSqlQuery adopt(h);
                adopt.prepare("UPDATE folders SET uid = ? WHERE id = ?");
                adopt.addBindValue(r.uid);
                adopt.addBindValue(localId);
                adopt.exec();
                QSqlQuery drop(h);
                drop.prepare("DELETE FROM sync_outbox WHERE kind = 'folder' AND uid = ?");
                drop.addBindValue(oldUid);
                drop.exec();
                localAt = 0;
            }
        }
        if (localAt < 0) {
            q.prepare("INSERT INTO folders(uid, parent_id, name, sort, prior_sec, color, created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
            q.addBindValue(r.uid);
        } else {
            q.prepare("UPDATE folders SET parent_id = ?, name = ?, sort = ?, prior_sec = ?, color = ?, created_at = ?, updated_at = ? WHERE uid = ?");
        }
        q.addBindValue(parent > 0 ? QVariant(parent) : QVariant(QMetaType(QMetaType::LongLong)));
        q.addBindValue(d.value("name").toString());
        q.addBindValue(d.value("sort").toInt());
        q.addBindValue(qint64(d.value("prior_sec").toDouble()));
        q.addBindValue(d.value("color").toString());
        q.addBindValue(qint64(d.value("created_at").toDouble()));
        q.addBindValue(r.updatedAt);
        if (localAt >= 0) q.addBindValue(r.uid);
    } else if (r.kind == QLatin1String("pomodoro")) {
        const qint64 folder = idOf("folders", d.value("folder").toString());
        const QString device = d.value("device").toString();
        const QString mine = deviceId();
        if (localAt < 0) {
            q.prepare("INSERT INTO pomodoros(uid, folder_id, day, started_at, ended_at, planned_sec, studied_sec, status, linked, note, device, updated_at) "
                      "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            q.addBindValue(r.uid);
        } else {
            q.prepare("UPDATE pomodoros SET folder_id = ?, day = ?, started_at = ?, ended_at = ?, planned_sec = ?, studied_sec = ?, status = ?, "
                      "linked = ?, note = ?, device = ?, updated_at = ? WHERE uid = ?");
        }
        q.addBindValue(folder > 0 ? QVariant(folder) : QVariant(QMetaType(QMetaType::LongLong)));
        q.addBindValue(d.value("day").toString());
        q.addBindValue(qint64(d.value("started_at").toDouble()));
        q.addBindValue(qint64(d.value("ended_at").toDouble()));
        q.addBindValue(d.value("planned_sec").toInt());
        q.addBindValue(d.value("studied_sec").toInt());
        q.addBindValue(d.value("status").toInt());
        q.addBindValue(d.value("linked").toBool() ? 1 : 0);
        q.addBindValue(d.value("note").toString());
        q.addBindValue(device == mine ? QString() : (device.isEmpty() ? QStringLiteral("remoto") : device));
        q.addBindValue(r.updatedAt);
        if (localAt >= 0) q.addBindValue(r.uid);
    } else if (r.kind == QLatin1String("interval")) {
        if (localAt < 0) {
            q.prepare("INSERT INTO intervals(uid, kind, day, started_at, seconds, updated_at) VALUES(?, ?, ?, ?, ?, ?)");
            q.addBindValue(r.uid);
        } else {
            q.prepare("UPDATE intervals SET kind = ?, day = ?, started_at = ?, seconds = ?, updated_at = ? WHERE uid = ?");
        }
        q.addBindValue(d.value("kind").toInt());
        q.addBindValue(d.value("day").toString());
        q.addBindValue(qint64(d.value("started_at").toDouble()));
        q.addBindValue(qint64(d.value("seconds").toDouble()));
        q.addBindValue(r.updatedAt);
        if (localAt >= 0) q.addBindValue(r.uid);
    } else if (r.kind == QLatin1String("goal")) {
        // "3x30@<uid>" → "3x30@<id local>"
        static const QRegularExpression atUid(QStringLiteral("@([0-9a-f]{32})"));
        const QString spec = d.value("spec").toString();
        QString local;
        qsizetype last = 0;
        for (auto m = atUid.globalMatch(spec); m.hasNext();) {
            const auto mm = m.next();
            local += spec.mid(last, mm.capturedStart() - last);
            const qint64 id = idOf("folders", mm.captured(1));
            if (id > 0) local += QStringLiteral("@%1").arg(id);
            last = mm.capturedEnd();
        }
        local += spec.mid(last);
        if (localAt < 0) {
            q.prepare("INSERT INTO goals(uid, title, spec, start_day, end_day, weekdays, created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
            q.addBindValue(r.uid);
        } else {
            q.prepare("UPDATE goals SET title = ?, spec = ?, start_day = ?, end_day = ?, weekdays = ?, created_at = ?, updated_at = ? WHERE uid = ?");
        }
        q.addBindValue(d.value("title").toString());
        q.addBindValue(local);
        q.addBindValue(d.value("start_day").toString());
        q.addBindValue(nullable(d.value("end_day")));
        q.addBindValue(d.value("weekdays").toInt());
        q.addBindValue(qint64(d.value("created_at").toDouble()));
        q.addBindValue(r.updatedAt);
        if (localAt >= 0) q.addBindValue(r.uid);
    } else if (isDay) {
        q.prepare("INSERT INTO days(day, title, updated_at) VALUES(?, ?, ?) ON CONFLICT(day) DO UPDATE SET title = excluded.title, updated_at = excluded.updated_at");
        q.addBindValue(r.uid);
        q.addBindValue(d.value("title").toString());
        q.addBindValue(r.updatedAt);
    }
    const bool ok = q.exec();
    if (ok) dropOutbox();
    return ok;
}

// ===========================================================================
// SyncClient
// ===========================================================================
SyncClient::SyncClient(Database* db, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_store(*db)
{
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &SyncClient::syncNow);
    if (signedIn()) m_status = QStringLiteral("Conectado");
}

bool SyncClient::signedIn() const { return !m_db->setting("sync/refresh").toString().isEmpty(); }
QString SyncClient::email() const { return m_db->setting("sync/email").toString(); }

QNetworkRequest SyncClient::request(const QString& path, const QString& token) const
{
    QNetworkRequest req(QUrl(QString::fromLatin1(syncconfig::kBaseUrl) + path));
    req.setRawHeader("apikey", syncconfig::kPublishableKey);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!token.isEmpty()) req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    req.setTransferTimeout(30000);
    return req;
}

void SyncClient::signIn(const QString& email, const QString& password)
{
    auth(QStringLiteral("/auth/v1/token?grant_type=password"), {{"email", email.trimmed()}, {"password", password}}, false);
}

void SyncClient::signUp(const QString& email, const QString& password)
{
    auth(QStringLiteral("/auth/v1/signup"), {{"email", email.trimmed()}, {"password", password}}, true);
}

void SyncClient::signOut()
{
    for (const char* k : {"sync/refresh", "sync/access", "sync/expires", "sync/email"}) m_db->setSetting(k, QString());
    m_db->setSetting("sync/seq", 0);
    m_status = QStringLiteral("Sin conectar");
    emit changed();
}

void SyncClient::auth(const QString& path, const QJsonObject& body, bool signup)
{
    m_status = QStringLiteral("Conectando…");
    emit changed();
    QNetworkReply* rep = m_net.post(request(path), QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(rep, &QNetworkReply::finished, this, [this, rep, body, signup] {
        rep->deleteLater();
        const QJsonObject o = QJsonDocument::fromJson(rep->readAll()).object();
        const QString access = o.value("access_token").toString();
        if (access.isEmpty()) {
            QString why = o.value("msg").toString(o.value("error_description").toString(o.value("message").toString()));
            if (signup && rep->error() == QNetworkReply::NoError) {
                m_status = QStringLiteral("Revisá tu mail");
                emit message(QStringLiteral("Te mandamos un mail para confirmar la cuenta. Confirmala y después tocá «Entrar»."));
            } else {
                if (why.contains(QLatin1String("Invalid login"), Qt::CaseInsensitive)) why = QStringLiteral("Mail o contraseña incorrectos");
                else if (why.contains(QLatin1String("not confirmed"), Qt::CaseInsensitive)) why = QStringLiteral("Todavía no confirmaste el mail");
                else if (why.isEmpty()) why = rep->errorString();
                m_status = QStringLiteral("No se pudo conectar");
                emit message(why);
            }
            emit changed();
            return;
        }
        m_db->setSetting("sync/access", access);
        m_db->setSetting("sync/refresh", o.value("refresh_token").toString());
        m_db->setSetting("sync/expires", QDateTime::currentSecsSinceEpoch() + o.value("expires_in").toInt(3600) - 60);
        if (body.contains("email")) m_db->setSetting("sync/email", body.value("email").toString());
        m_status = QStringLiteral("Conectado");
        emit changed();
        syncNow();
    });
}

void SyncClient::withToken(std::function<void(const QString&)> fn)
{
    const QString access = m_db->setting("sync/access").toString();
    if (!access.isEmpty() && QDateTime::currentSecsSinceEpoch() < m_db->setting("sync/expires").toLongLong()) {
        fn(access);
        return;
    }
    const QString refresh = m_db->setting("sync/refresh").toString();
    if (refresh.isEmpty()) { finish(QStringLiteral("Sin conectar")); return; }
    QNetworkReply* rep = m_net.post(request(QStringLiteral("/auth/v1/token?grant_type=refresh_token")),
                                    QJsonDocument(QJsonObject{{"refresh_token", refresh}}).toJson(QJsonDocument::Compact));
    connect(rep, &QNetworkReply::finished, this, [this, rep, fn] {
        rep->deleteLater();
        const int code = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject o = QJsonDocument::fromJson(rep->readAll()).object();
        const QString access = o.value("access_token").toString();
        if (access.isEmpty()) {
            if (code == 400 || code == 401) {   // la sesión se venció de verdad: hay que volver a entrar
                m_db->setSetting("sync/refresh", QString());
                finish(QStringLiteral("Volvé a entrar"));
            } else {
                finish(QStringLiteral("Sin internet: se sincroniza después"));
            }
            return;
        }
        m_db->setSetting("sync/access", access);
        m_db->setSetting("sync/refresh", o.value("refresh_token").toString());
        m_db->setSetting("sync/expires", QDateTime::currentSecsSinceEpoch() + o.value("expires_in").toInt(3600) - 60);
        fn(access);
    });
}

void SyncClient::syncNow()
{
    if (!signedIn()) return;
    if (m_busy) { m_again = true; return; }
    m_busy = true;
    m_status = QStringLiteral("Sincronizando…");
    emit changed();
    withToken([this](const QString& token) { pull(token); });
}

void SyncClient::pull(const QString& token)
{
    const qint64 seq = m_db->setting("sync/seq", 0).toLongLong();
    QUrlQuery qq;
    qq.addQueryItem(QStringLiteral("select"), QStringLiteral("kind,uid,data,updated_at,deleted,seq"));
    qq.addQueryItem(QStringLiteral("seq"), QStringLiteral("gt.%1").arg(seq));
    qq.addQueryItem(QStringLiteral("order"), QStringLiteral("seq.asc"));
    qq.addQueryItem(QStringLiteral("limit"), QStringLiteral("500"));
    QNetworkReply* rep = m_net.get(request(QStringLiteral("/rest/v1/records?") + qq.toString(QUrl::FullyEncoded), token));
    connect(rep, &QNetworkReply::finished, this, [this, rep, token] {
        rep->deleteLater();
        if (rep->error() != QNetworkReply::NoError) { finish(QStringLiteral("Sin internet: se sincroniza después")); return; }
        const QJsonArray arr = QJsonDocument::fromJson(rep->readAll()).array();
        QVector<SyncRecord> in;
        qint64 maxSeq = m_db->setting("sync/seq", 0).toLongLong();
        for (const QJsonValue& v : arr) {
            const QJsonObject o = v.toObject();
            SyncRecord r;
            r.kind = o.value("kind").toString();
            r.uid = o.value("uid").toString();
            r.data = o.value("data").toObject();
            r.updatedAt = qint64(o.value("updated_at").toDouble());
            r.deleted = o.value("deleted").toBool();
            r.seq = qint64(o.value("seq").toDouble());
            maxSeq = qMax(maxSeq, r.seq);
            in.append(r);
        }
        const int changed = in.isEmpty() ? 0 : m_store.apply(in);
        m_db->setSetting("sync/seq", maxSeq);
        if (changed > 0) emit applied(changed);
        if (arr.size() >= 500) { pull(token); return; }   // había más
        push(token);
    });
}

void SyncClient::push(const QString& token)
{
    const QVector<SyncRecord> out = m_store.pending(300);
    if (out.isEmpty()) { finish(QStringLiteral("Sincronizado %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm")))); return; }
    QJsonArray items;
    for (const SyncRecord& r : out)
        items.append(QJsonObject{{"kind", r.kind}, {"uid", r.uid}, {"data", r.deleted ? QJsonValue() : QJsonValue(r.data)},
                                 {"updated_at", r.updatedAt}, {"deleted", r.deleted}});
    QNetworkReply* rep = m_net.post(request(QStringLiteral("/rest/v1/rpc/push_records"), token),
                                    QJsonDocument(QJsonObject{{"items", items}}).toJson(QJsonDocument::Compact));
    connect(rep, &QNetworkReply::finished, this, [this, rep, out, token] {
        rep->deleteLater();
        if (rep->error() != QNetworkReply::NoError) {
            const QString body = QString::fromUtf8(rep->readAll()).left(200);
            finish(body.contains(QLatin1String("push_records")) ? QStringLiteral("Falta configurar Supabase (tools/supabase.sql)")
                                                                : QStringLiteral("Sin internet: se sincroniza después"));
            return;
        }
        m_store.acknowledge(out);
        if (out.size() >= 300) { push(token); return; }   // quedaban más
        finish(QStringLiteral("Sincronizado %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm"))));
    });
}

void SyncClient::finish(const QString& status)
{
    m_busy = false;
    m_status = status;
    emit changed();
    if (m_again) { m_again = false; QTimer::singleShot(500, this, &SyncClient::syncNow); }
}

void SyncClient::setAutoSync(bool on, int intervalMs)
{
    if (on) { m_timer.start(intervalMs); syncNow(); }
    else m_timer.stop();
}
