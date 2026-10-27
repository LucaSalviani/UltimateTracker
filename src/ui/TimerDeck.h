#pragma once

#include <QTimer>
#include <QWidget>

class QLabel;
class QLineEdit;
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
        bool    canStart = true;     // false viendo otro día: solo se registra hoy
    };

    // Formas de ajustar la duración antes de arrancar (Ajustes › Cronómetro).
    struct AdjustOptions {
        bool wheel = true;       // rueda del mouse sobre el tiempo
        int  wheelStep = 1;      // minutos por paso de rueda
        bool type = true;        // click en el tiempo para escribirlo
        bool buttons = true;     // botones de paso fino a los costados
        int  stepSmall = 1;      // botones de afuera
        int  stepBig = 5;        // botones de adentro
    };

    explicit TimerDeck(QWidget* parent = nullptr);
    void setModel(const Model& m);
    void setAdjustOptions(const AdjustOptions& o);
    // M31: visor del color del acento (0), oscuro (1) o siguiendo al tema (2).
    void setVisorStyle(int style);
    // Nota del pomodoro que acaba de terminar (se muestra en "¿recreo?").
    void setNoteEnabled(bool on);
    void clearNote(const QString& placeholderHint = {});
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
    void goTodayClicked();
    void noteEdited(const QString& text);   // Enter o al salir de "¿recreo?"

protected:
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    void applyMode();
    bool editable() const;
    void nudge(int minutes);           // suma/resta y emite durationChanged
    void beginTyping();
    void finishTyping(bool commit);

    Model m_model;
    AdjustOptions m_adjust;
    bool  m_first = true;

    QToolButton* m_activity;
    DeckDisplay* m_display;
    QWidget*     m_stepper;
    QToolButton* m_minus;
    QToolButton* m_plus;
    QToolButton* m_minusFine;   // botones de afuera (paso chico)
    QToolButton* m_plusFine;
    QLabel*      m_stepLabel;
    QLineEdit*   m_editor;      // escribir los minutos sobre el visor
    QLineEdit*   m_note;        // ¿en qué trabajaste?
    bool         m_noteEnabled = true;
    QString      m_noteSaved;
    int          m_wheelAccum = 0;
    QPushButton* m_secondary;
    QToolButton* m_primary;
};

// Pantalla del reproductor (pintada a mano).
class DeckDisplay : public QWidget {
    Q_OBJECT
public:
    explicit DeckDisplay(QWidget* parent = nullptr);
    void setModel(const TimerDeck::Model& m);
    // Zona de los dígitos grandes (para poner el editor encima).
    QRectF digitsRect() const;

signals:
    void wheelTurned(int angleDelta);
    void digitsClicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;

private:
    void paintReel(class QPainter& p, const QPointF& c, qreal r, bool spinning) const;

    TimerDeck::Model m_model;
    QTimer m_anim;      // carretes girando y parpadeos
    qreal  m_phase = 0;
};
