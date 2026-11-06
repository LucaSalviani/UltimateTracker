#pragma once

#include <QColor>
#include <QDate>
#include <QWidget>

class Database;
class QLabel;
class QPushButton;
class QGraphicsOpacityEffect;

// Tarjeta del día, sobre la misma ventana de UltimateTracker (Mejoras 2.0: antes
// era una ventana aparte y se pisaba con el resumen del día anterior).
//
// Aparece una vez por día, la primera vez que mirás la app:
//  - con el saludo (si está activado en Ajustes): "Buen día, ¿listo para…?" + frase;
//  - si no, como resumen del último día que estudiaste.
// En los dos casos: cómo te fue, la racha y los comodines que quedan (con aviso
// si hoy se corta), el objetivo de hoy y con qué actividad arrancás, para
// cambiarla ahí mismo si no es esa.
class Greeting : public QWidget {
    Q_OBJECT
public:
    // full = con saludo y frase; si no, solo el resumen.
    Greeting(Database* db, bool full, QWidget* parent);

    // ¿Corresponde saludar? (el ajuste está activo y hoy todavía no se saludó)
    static bool shouldShow(Database& db);
    // ¿Hay un día anterior sin resumir?
    static bool summaryPending(Database& db);

    void setActivity(const QString& name, const QColor& color);
    void popIn();

signals:
    void accepted();                              // "A trabajar"
    void declined();                              // "Ahora no" / Esc
    void activityMenuRequested(const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void finish(bool accepted);

    Database*    m_db;
    QWidget*     m_card;
    QPushButton* m_activity;
    QGraphicsOpacityEffect* m_fade;
    bool         m_closing = false;
};
