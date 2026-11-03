#pragma once

#include <QJsonObject>
#include <functional>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

class Database;

// Celu-3/4: sincronización entre la PC y el celu.
//
// Cada fila de actividades, pomodoros, recreos, objetivos y títulos de días tiene
// un id global (uid) y la hora de su último cambio. La base anota sola en
// sync_outbox lo que cambia (triggers, ver Database::setupSync). Para sincronizar:
//   1. se baja del servidor lo nuevo desde la última vez y se aplica
//      (gana el cambio más reciente; si lo local es más nuevo, se queda lo local);
//   2. se sube lo anotado en sync_outbox.
// El servidor es una tabla "records" en Supabase (ver tools/supabase.sql).

struct SyncRecord {
    QString     kind;        // folder, pomodoro, interval, goal, day
    QString     uid;
    QJsonObject data;
    qint64      updatedAt = 0;
    bool        deleted = false;
    qint64      seq = 0;     // orden en el servidor
};

// La parte local (sin red): qué hay para mandar y cómo aplicar lo que llega. Testeable.
class SyncStore {
public:
    explicit SyncStore(Database& db);
    QString deviceId();                              // id de este aparato (se crea la primera vez)
    QVector<SyncRecord> pending(int limit = 200);
    void acknowledge(const QVector<SyncRecord>& sent);   // ya están en el servidor
    int  apply(const QVector<SyncRecord>& incoming);     // devuelve cuántos cambiaron algo
    int  pendingCount();

private:
    QString uidOf(const char* table, qint64 id);
    qint64  idOf(const char* table, const QString& uid);
    bool    applyOne(const SyncRecord& r);
    Database& m_db;
};

// La parte de red: inicio de sesión y idas y vueltas con Supabase.
class SyncClient : public QObject {
    Q_OBJECT
public:
    explicit SyncClient(Database* db, QObject* parent = nullptr);

    bool    signedIn() const;
    QString email() const;
    QString status() const { return m_status; }
    bool    busy() const { return m_busy; }

    void signIn(const QString& email, const QString& password);
    void signUp(const QString& email, const QString& password);
    void signOut();
    void syncNow();
    void setAutoSync(bool on, int intervalMs = 60000);

    // En vivo: una sola fila ("live"/"session") con el estado del reloj.
    // Se pregunta seguido (cada pocos segundos) y se manda al tocar algo.
    void fetchLive();
    void pushLive(const QJsonObject& data, qint64 version);
    QString deviceId() { return m_store.deviceId(); }

signals:
    void changed();                 // sesión, estado o mensaje
    void applied(int count);        // llegaron cambios: recargar la interfaz
    void message(const QString& text);
    void liveReceived(const QJsonObject& data);   // vacío = todavía no hay nada en el servidor
    void livePushed(bool ok);

private:
    void auth(const QString& path, const QJsonObject& body, bool signup);
    void withToken(std::function<void(const QString& token)> fn);
    void pull(const QString& token);
    void push(const QString& token);
    void finish(const QString& status);
    QNetworkRequest request(const QString& path, const QString& token = {}) const;

    Database*             m_db;
    SyncStore             m_store;
    QNetworkAccessManager m_net;
    QTimer                m_timer;
    QString               m_status;
    bool                  m_busy = false;
    bool                  m_again = false;
    bool                  m_liveBusy = false;
    QVector<std::function<void(const QString&)>> m_waiting;   // esperando un token nuevo
};
