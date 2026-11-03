#pragma once

#include "core/SessionController.h"

#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <functional>

class Database;
class SyncClient;

// En vivo (PC ↔ celu, como Spotify): el reloj es uno solo.
//
// En el servidor hay una fila con la foto del reloj: estado, pomodoro, cuánto
// llevaba y a qué hora se sacó la foto, más un número de versión. Cada aparato:
//  - al tocar algo (play, pausa, terminar, recreo…) manda una foto nueva con
//    versión + 1 (el servidor solo la acepta si es más nueva que la que tiene);
//  - pregunta cada pocos segundos; si hay una versión más nueva de otro aparato,
//    la copia: el reloj de acá queda "espejo" (cuenta igual, sin guardar nada).
//  - Si tocás algo en el espejo, ese aparato toma el reloj y sigue desde ahí.
// Si los dos tocan a la vez, gana el primero que llega al servidor y el otro se
// pone al día en la siguiente pregunta.
class LiveLink : public QObject {
    Q_OBJECT
public:
    using Clock = std::function<qint64()>;   // ms (los tests usan uno simulado)
    LiveLink(Database* db, SessionController* session, SyncClient* sync, const QString& label,
             Clock clock = {}, QObject* parent = nullptr);

    void setPollInterval(int ms);   // 0 = no preguntar solo
    void pollNow();
    bool mirroring() const;         // el reloj lo maneja el otro aparato
    QString remoteLabel() const { return m_remoteLabel; }   // "la PC" / "el celu"

    // Para probar sin red: lo que se mandaría y lo que llega.
    QJsonObject snapshot() const;
    void receive(const QJsonObject& data);
    void flush();                   // manda ya lo que estaba por mandarse

signals:
    void changed();
    void published(const QJsonObject& data, qint64 version);

private:
    void schedulePublish();
    void publish();
    void applyRemote(const QJsonObject& data, bool announce);

    Database*          m_db;
    SessionController* m_session;
    SyncClient*        m_sync;
    QString            m_label;
    QString            m_device;
    QTimer             m_poll;
    QTimer             m_debounce;
    qint64             m_v = 0;          // última versión conocida
    QString            m_owner;          // aparato que mandó esa versión
    bool               m_ready = false;  // ya se preguntó al menos una vez
    bool               m_pending = false;
    bool               m_applying = false;
    QJsonObject        m_last;           // última foto aceptada de otro aparato
    QString            m_remoteLabel;
    Clock              m_clock;
};
