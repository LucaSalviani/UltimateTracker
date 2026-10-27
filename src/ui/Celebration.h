#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

// Animación de logro (M8): tapa la ventana un par de segundos y se va sola.
// Inspirada en el "visor" de una cámara (anillo con marcas que se dibuja) y en
// las maestrías de LoL (emblema que entra con rebote + destello).
// Un click la cierra antes.
class Celebration : public QWidget {
    Q_OBJECT
public:
    enum class Kind {
        Pomodoro,    // terminó un pomodoro
        Goal,        // meta del día cumplida
        Milestone,   // hito de horas (25 h, 50 h, …)
    };

    explicit Celebration(QWidget* parent);
    void play(Kind kind, const QString& kicker, const QString& title, const QString& subtitle);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    void finish();
    qint64 elapsed() const { return m_clock.elapsed() + m_skipMs; }

    Kind          m_kind = Kind::Pomodoro;
    QString       m_kicker, m_title, m_subtitle;
    QColor        m_accent;
    QElapsedTimer m_clock;
    QTimer        m_frame;
    int           m_duration = 2400;
    qint64        m_skipMs = 0;   // para capturas de prueba
};
