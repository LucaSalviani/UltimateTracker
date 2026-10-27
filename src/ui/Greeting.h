#pragma once

#include <QRect>
#include <QWidget>

class Database;
class QLabel;
class QPushButton;
class QGraphicsOpacityEffect;

// M10: saludo al iniciar con Windows (una vez por día).
//  "A trabajar"   → la tarjeta se expande hasta el lugar de la ventana y se abre
//                   la app con el panel de la meta del día.
//  "Por ahora no" → cierra el programa.
class Greeting : public QWidget {
    Q_OBJECT
public:
    Greeting(Database* db, const QRect& appGeometry);

    // ¿Corresponde saludar? (el ajuste está activo y hoy todavía no se saludó)
    static bool shouldShow(Database& db);

signals:
    void accepted();
    void declined();

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;

private:
    void expandAndOpen();

    Database* m_db;
    QRect     m_target;     // donde va a aparecer la ventana principal
    QWidget*  m_content;
    QGraphicsOpacityEffect* m_fade;
    QPoint    m_dragOffset;
    bool      m_expanding = false;
};
