#pragma once

#include "ui/TimerDeck.h"

#include <QWidget>

class QToolButton;
class QLabel;

// M19: mini reproductor flotante. Ventanita siempre visible (≈ 1/3 del
// reproductor grande) con lo justo: pomodoro, actividad, tiempo y los botones
// de play/pausa, terminar y saltear recreo. Se arrastra desde cualquier lado.
class MiniPlayer : public QWidget {
    Q_OBJECT
public:
    explicit MiniPlayer(QWidget* parent = nullptr);
    void setModel(const TimerDeck::Model& m);

signals:
    void playClicked();
    void pauseClicked();
    void endPomodoroClicked();
    void startBreakClicked();
    void skipBreakClicked();
    void endBreakClicked();
    void silenceClicked();
    void openAppClicked();
    void moved(const QPoint& pos);
    void closed();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    void applyMode();

    TimerDeck::Model m_model;
    bool         m_first = true;
    QToolButton* m_primary;
    QToolButton* m_secondary;
    QToolButton* m_close;
    QPoint       m_dragOffset;
    bool         m_dragging = false;
};
