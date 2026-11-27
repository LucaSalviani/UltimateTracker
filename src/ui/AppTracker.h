#pragma once

#include <QDate>
#include <QHash>
#include <QObject>
#include <QTimer>

class Database;

// Mejoras 7.0: anota qué programa y ventana están al frente (cada 5 s, con detección de inactividad)
// mientras la app está abierta, aunque esté en segundo plano. Cuesta casi nada: una consulta a Windows
// cada 5 s y un guardado por minuto. Apagado por defecto (Ajustes › Sistema).
class AppTracker : public QObject {
    Q_OBJECT
public:
    AppTracker(Database* db, QObject* parent = nullptr);
    void setEnabled(bool on);
    void setInPomodoro(bool on) { m_inPomo = on; }
    bool enabled() const { return m_on; }
    void flush();                   // guarda lo acumulado (al salir, o antes de mirar los gráficos)

private:
    void poll();
    Database*   m_db;
    QTimer      m_timer;
    bool        m_on = false;
    bool        m_inPomo = false;
    qint64      m_last = 0;
    int         m_sinceFlush = 0;
    struct Key { QString exe, title; bool pomo; QDate day; bool operator==(const Key& o) const { return exe == o.exe && title == o.title && pomo == o.pomo && day == o.day; } };
    QVector<QPair<Key, qint64>> m_pending;
};
