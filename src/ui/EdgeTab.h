#pragma once

#include <QVariantAnimation>
#include <QWidget>

// Pestañita fina pegada a un borde de la pantalla (estilo Samsung Edge).
// No muestra nada: solo está ahí para volver a abrir la ventana con un click.
class EdgeTab : public QWidget {
    Q_OBJECT
public:
    enum class Edge { Left, Right, Top, Bottom };

    explicit EdgeTab(QWidget* parent = nullptr);

    // Se ubica sobre `edge` de la pantalla, centrada en `along` (coordenada x o y global).
    void attach(Edge edge, const QRect& screenGeometry, int along);
    Edge edge() const { return m_edge; }

signals:
    void activated();

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    void animateGrow(qreal to);

    Edge m_edge = Edge::Right;
    qreal m_grow = 0.0;             // 0 = en reposo, 1 = desplegada por el hover
    QVariantAnimation m_growAnim;
};
