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

    // M26: estilos. Visor = el de siempre; los otros son minimalistas y no tapan nada.
    enum class Style { Visor = 0, Line = 1, Border = 2, Tick = 3, Figure = 4 };
    static void setStyle(Style s);                 // el mismo para todo
    static void setStyleFor(Kind k, Style s);      // M31: uno por cosa
    static Style style();
    static Style styleFor(Kind k);
    static QStringList styleNames();

    explicit Celebration(QWidget* parent);
    void play(Kind kind, const QString& kicker, const QString& title, const QString& subtitle);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    void finish();
    void paintVisor(class QPainter& p, qreal ms);
    void paintLine(class QPainter& p, qreal ms);
    void paintBorder(class QPainter& p, qreal ms);
    void paintTick(class QPainter& p, qreal ms);
    void paintFigure(class QPainter& p, qreal ms);
    qint64 elapsed() const { return m_clock.elapsed() + m_skipMs; }

    Kind          m_kind = Kind::Pomodoro;
    Style         m_style = Style::Visor;
    QString       m_kicker, m_title, m_subtitle;
    QColor        m_accent;
    QElapsedTimer m_clock;
    QTimer        m_frame;
    int           m_duration = 2400;
    qint64        m_skipMs = 0;   // para capturas de prueba
};
