#pragma once

#include <QTimer>
#include <QWidget>

class QLabel;
class QMenu;
class QPushButton;
class QToolButton;
class DeckDisplay;

// El "reproductor": cronómetro grande con todos los controles que hacen avanzar,
// cambiar o mover el tiempo. Inspirado en las caseteras y DVDs viejos, pero
// minimalista. Rojo = pomodoro, verde agua = recreo.
class TimerDeck : public QWidget {
    Q_OBJECT
public:
    enum class Mode {
        Ready,       // listo para el próximo pomodoro
        Running,     // pomodoro corriendo
        Paused,      // pomodoro en pausa
        BreakReady,  // terminó el pomodoro: ¿recreo?
        Break,       // recreo corriendo
        BreakOver,   // terminó el recreo (alarma)
    };

    struct Model {
        Mode    mode = Mode::Ready;
        int     number = 1;          // número de pomodoro del día (el actual o el próximo)
        QString activity;            // ruta de la actividad
        int     remainingSec = 0;
        int     totalSec = 0;
        int     durationMin = 25;    // lo que ajusta el − / + (pomodoro o recreo)
    };

    explicit TimerDeck(QWidget* parent = nullptr);
    void setModel(const Model& m);
    void setActivityMenu(QMenu* menu);

signals:
    void playClicked();          // arrancar pomodoro / seguir / siguiente
    void pauseClicked();
    void endPomodoroClicked();
    void startBreakClicked();
    void skipBreakClicked();
    void endBreakClicked();
    void silenceClicked();
    void durationChanged(int minutes);

private:
    void applyMode();

    Model m_model;
    bool  m_first = true;

    QToolButton* m_activity;
    DeckDisplay* m_display;
    QWidget*     m_stepper;
    QToolButton* m_minus;
    QToolButton* m_plus;
    QLabel*      m_stepLabel;
    QPushButton* m_secondary;
    QToolButton* m_primary;
};

// Pantalla del reproductor (pintada a mano).
class DeckDisplay : public QWidget {
    Q_OBJECT
public:
    explicit DeckDisplay(QWidget* parent = nullptr);
    void setModel(const TimerDeck::Model& m);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void paintReel(class QPainter& p, const QPointF& c, qreal r, bool spinning) const;

    TimerDeck::Model m_model;
    QTimer m_anim;      // carretes girando y parpadeos
    qreal  m_phase = 0;
};
