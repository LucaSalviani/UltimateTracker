#pragma once

#include <QPointer>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;

// Mejoras 2.0: mini tutorial. Sale la primera vez que se abre la app y cuando
// lo pedís desde ⋯ › Ver tutorial. Oscurece todo menos la parte que explica y
// muestra un globito con texto y Anterior / Siguiente / Saltar.
class Tour : public QWidget {
    Q_OBJECT
public:
    struct Step {
        QPointer<QWidget> target;   // nullptr = globito centrado (sin foco)
        QString title;
        QString text;
    };
    Tour(const QVector<Step>& steps, QWidget* parent);
    void start();

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;

private:
    void go(int index);
    void place();
    QRect hole() const;

    QVector<Step> m_steps;
    int           m_index = 0;
    QWidget*      m_bubble;
    QLabel*       m_count;
    QLabel*       m_title;
    QLabel*       m_text;
    QPushButton*  m_prev;
    QPushButton*  m_next;
    QPushButton*  m_skip;
};
