#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QWidget>

class QLabel;

// Mejoras 3.0: distracciones. Con un pomodoro corriendo, mira qué ventana está
// al frente (el título y el programa). Si coincide con la lista (Instagram,
// WhatsApp…) más tiempo que el de gracia, avisa. Todo local: no sale nada de
// la PC y no se guarda qué mirabas.
// Límites: solo la ventana al frente; no ve una pestaña de fondo ni el celu.
class FocusGuard : public QObject {
    Q_OBJECT
public:
    explicit FocusGuard(QObject* parent = nullptr);
    void configure(bool on, int graceSec, const QStringList& words);
    void setWatching(bool on);     // true mientras corre un pomodoro propio
    void snooze(int seconds);      // "era necesario": no avisa por un rato
    static QStringList defaultWords() { return {QStringLiteral("Instagram"), QStringLiteral("WhatsApp")}; }
    // Para probar: qué palabra de la lista aparece en ese título/programa ("" = ninguna)
    static QString match(const QString& title, const QString& exe, const QStringList& words);

signals:
    void distracted(const QString& what);   // pasó el tiempo de gracia

private:
    void poll();
    bool        m_on = false;
    bool        m_watching = false;
    int         m_graceSec = 20;
    QStringList m_words;
    QTimer      m_timer;
    int         m_awaySec = 0;
    qint64      m_snoozeUntil = 0;
    bool        m_fired = false;
};

// Cartel rojo a pantalla completa: "RECREO INDEBIDO".
class FocusAlarm : public QWidget {
    Q_OBJECT
public:
    explicit FocusAlarm(QWidget* parent = nullptr);
    void showFor(const QString& what, bool paused);

signals:
    void backToWork();        // "Volver al pomodoro"
    void needed();            // "Era necesario" (no avisa por 10 minutos)

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    QLabel* m_what;
    QLabel* m_clock;
    QTimer  m_tick;
    qint64  m_since = 0;
};
