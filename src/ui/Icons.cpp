#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace icons {
namespace {

// Dibuja en un lienzo lógico de 24×24.
void draw(QPainter& p, Id id, const QColor& c)
{
    QPen pen(c, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (id) {
    case Id::Play: {
        QPainterPath path;
        path.moveTo(8.5, 6);
        path.lineTo(18.5, 12);
        path.lineTo(8.5, 18);
        path.closeSubpath();
        p.setBrush(c);
        p.drawPath(path);
        break;
    }
    case Id::Pause:
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(7, 6, 3.6, 12), 1, 1);
        p.drawRoundedRect(QRectF(13.4, 6, 3.6, 12), 1, 1);
        break;
    case Id::Stop:
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(7, 7, 10, 10), 1.6, 1.6);
        break;
    case Id::ChevronLeft:
        p.drawPolyline(QPolygonF({{14.5, 6.5}, {9, 12}, {14.5, 17.5}}));
        break;
    case Id::ChevronRight:
        p.drawPolyline(QPolygonF({{9.5, 6.5}, {15, 12}, {9.5, 17.5}}));
        break;
    case Id::Back:
        p.drawPolyline(QPolygonF({{11, 6.5}, {5.5, 12}, {11, 17.5}}));
        p.drawLine(QPointF(6, 12), QPointF(18.5, 12));
        break;
    case Id::ZoomOut:   // de día → semana → mes (más cosas a la vista)
        p.drawRect(QRectF(5, 5, 5.5, 5.5));
        p.drawRect(QRectF(13.5, 5, 5.5, 5.5));
        p.drawRect(QRectF(5, 13.5, 5.5, 5.5));
        p.drawRect(QRectF(13.5, 13.5, 5.5, 5.5));
        break;
    case Id::ZoomIn:    // volver a la vista más cercana
        p.drawRect(QRectF(6, 6, 12, 12));
        break;
    case Id::Proportional:
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawRect(QRectF(4, 13, 5, 5));
        p.drawRect(QRectF(10.5, 8, 9.5, 10));
        break;
    case Id::Stats:
        p.drawLine(QPointF(6, 18), QPointF(6, 13));
        p.drawLine(QPointF(10, 18), QPointF(10, 8));
        p.drawLine(QPointF(14, 18), QPointF(14, 11));
        p.drawLine(QPointF(18, 18), QPointF(18, 6));
        break;
    case Id::Menu:
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 3; ++i) p.drawEllipse(QPointF(6 + i * 6, 12), 1.5, 1.5);
        break;
    case Id::Pin: {
        p.drawLine(QPointF(12, 15), QPointF(12, 20));
        QPainterPath head;
        head.moveTo(8, 4.5);
        head.lineTo(16, 4.5);
        head.lineTo(15, 10);
        head.lineTo(17.5, 14.5);
        head.lineTo(6.5, 14.5);
        head.lineTo(9, 10);
        head.closeSubpath();
        p.drawPath(head);
        break;
    }
    case Id::Dock:      // mandar al borde
        p.drawLine(QPointF(19, 5), QPointF(19, 19));
        p.drawLine(QPointF(5, 12), QPointF(15, 12));
        p.drawPolyline(QPolygonF({{11, 8}, {15, 12}, {11, 16}}));
        break;
    case Id::Minimize:
        p.drawLine(QPointF(6.5, 12.5), QPointF(17.5, 12.5));
        break;
    case Id::Skip: {     // ⏭
        QPainterPath path;
        path.moveTo(6, 6.5);
        path.lineTo(14, 12);
        path.lineTo(6, 17.5);
        path.closeSubpath();
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawPath(path);
        p.drawRoundedRect(QRectF(15.2, 6.5, 2.8, 11), 0.8, 0.8);
        break;
    }
    case Id::Mute: {     // campana tachada
        QPainterPath bell;
        bell.moveTo(7, 16);
        bell.lineTo(17, 16);
        bell.lineTo(15.8, 14.2);
        bell.lineTo(15.8, 10.5);
        bell.cubicTo(15.8, 8, 14.2, 6.2, 12, 6.2);
        bell.cubicTo(9.8, 6.2, 8.2, 8, 8.2, 10.5);
        bell.lineTo(8.2, 14.2);
        bell.closeSubpath();
        p.drawPath(bell);
        p.drawLine(QPointF(10.6, 18.4), QPointF(13.4, 18.4));
        p.drawLine(QPointF(5.5, 5.5), QPointF(18.5, 18.5));
        break;
    }
    case Id::Close:
        p.drawLine(QPointF(7, 7), QPointF(17, 17));
        p.drawLine(QPointF(17, 7), QPointF(7, 17));
        break;
    case Id::Folder: {
        QPainterPath path;
        path.moveTo(4, 7.5);
        path.lineTo(9.5, 7.5);
        path.lineTo(11, 9.5);
        path.lineTo(20, 9.5);
        path.lineTo(20, 18);
        path.lineTo(4, 18);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Id::AppLogo: {
        p.setPen(Qt::NoPen);
        p.setBrush(theme::squareDone);
        p.drawRoundedRect(QRectF(2, 2, 20, 20), 4, 4);
        const QColor cells[4] = {theme::squareFill, theme::squareFill, theme::squareFill, theme::squareFinal};
        for (int i = 0; i < 4; ++i) {
            p.setBrush(cells[i]);
            p.drawRoundedRect(QRectF(5 + (i % 2) * 7.5, 5 + (i / 2) * 7.5, 6.5, 6.5), 1.2, 1.2);
        }
        break;
    }
    }
}

// Motor de íconos vectorial: cada vez que Qt necesita el ícono lo dibuja al
// tamaño exacto y a la densidad real de la pantalla (125 %, 150 %, 200 %…),
// así nunca se escala un bitmap y se ve nítido siempre.
class VectorIconEngine : public QIconEngine {
public:
    VectorIconEngine(Id id, const QColor& color, Ink ink) : m_id(id), m_color(color), m_ink(ink) {}

    QColor resolved() const
    {
        if (m_color.isValid()) return m_color;
        switch (m_ink) {
        case Ink::Text: return theme::text;
        case Ink::Dim:  return theme::textDim;
        default:        return theme::iconInk;
        }
    }

    void paint(QPainter* p, const QRect& rect, QIcon::Mode mode, QIcon::State) override
    {
        QColor c = resolved();
        if (mode == QIcon::Disabled) c.setAlphaF(c.alphaF() * 0.35);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->translate(rect.topLeft());
        p->scale(rect.width() / 24.0, rect.height() / 24.0);
        draw(*p, m_id, c);
        p->restore();
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QPixmap pm(size * scale);
        pm.fill(Qt::transparent);
        pm.setDevicePixelRatio(scale);
        QPainter p(&pm);
        paint(&p, QRect(QPoint(), size), mode, state);
        return pm;
    }

    QIconEngine* clone() const override { return new VectorIconEngine(m_id, m_color, m_ink); }
    // Sin caché: así un cambio de tema se ve al instante.
    QString key() const override { return QStringLiteral("VectorIconEngine"); }

private:
    Id     m_id;
    QColor m_color;
    Ink    m_ink;
};

} // namespace

QIcon get(Id id, Ink ink)
{
    return QIcon(new VectorIconEngine(id, QColor(), ink));
}

QIcon get(Id id, const QColor& fixedColor)
{
    return QIcon(new VectorIconEngine(id, fixedColor, Ink::Icon));
}

} // namespace icons
