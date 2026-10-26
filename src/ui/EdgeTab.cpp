#include "ui/EdgeTab.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

namespace {
// La ventana de la pestaña reserva lugar para la versión desplegada; lo que no
// se pinta es transparente y los clicks pasan a lo que haya debajo.
constexpr int   kThickness  = 24;
constexpr int   kLength     = 112;
constexpr qreal kRestThick  = 5;    // píldora en reposo
constexpr qreal kHoverThick = 16;   // píldora con el mouse encima
constexpr qreal kRestLen    = 84;
constexpr qreal kHoverLen   = 104;
constexpr qreal kHitThick   = 10;   // franja invisible que detecta el hover
}

EdgeTab::EdgeTab(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                          Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Abrir UltimateTracker"));

    m_growAnim.setDuration(140);
    m_growAnim.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_growAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_grow = v.toReal();
        update();
    });
}

void EdgeTab::animateGrow(qreal to)
{
    m_growAnim.stop();
    m_growAnim.setStartValue(m_grow);
    m_growAnim.setEndValue(to);
    m_growAnim.start();
}

void EdgeTab::attach(Edge edge, const QRect& s, int along)
{
    m_edge = edge;
    m_growAnim.stop();
    m_grow = 0.0;
    QRect r;
    switch (edge) {
    case Edge::Left:
        r = QRect(s.left(), 0, kThickness, kLength);
        r.moveCenter(QPoint(s.left() + kThickness / 2, qBound(s.top() + kLength / 2, along, s.bottom() - kLength / 2)));
        break;
    case Edge::Right:
        r = QRect(0, 0, kThickness, kLength);
        r.moveCenter(QPoint(s.right() - kThickness / 2, qBound(s.top() + kLength / 2, along, s.bottom() - kLength / 2)));
        break;
    case Edge::Top:
        r = QRect(0, 0, kLength, kThickness);
        r.moveCenter(QPoint(qBound(s.left() + kLength / 2, along, s.right() - kLength / 2), s.top() + kThickness / 2));
        break;
    case Edge::Bottom:
        r = QRect(0, 0, kLength, kThickness);
        r.moveCenter(QPoint(qBound(s.left() + kLength / 2, along, s.right() - kLength / 2), s.bottom() - kThickness / 2));
        break;
    }
    setGeometry(r);
}

// Rectángulo pegado al borde, de `thick` de espesor y `len` de largo, centrado.
static QRectF edgeRect(EdgeTab::Edge e, const QSizeF& sz, qreal thick, qreal len)
{
    const bool vertical = e == EdgeTab::Edge::Left || e == EdgeTab::Edge::Right;
    const qreal along = vertical ? (sz.height() - len) / 2 : (sz.width() - len) / 2;
    switch (e) {
    case EdgeTab::Edge::Left:   return QRectF(0, along, thick, len);
    case EdgeTab::Edge::Right:  return QRectF(sz.width() - thick, along, thick, len);
    case EdgeTab::Edge::Top:    return QRectF(along, 0, len, thick);
    case EdgeTab::Edge::Bottom: return QRectF(along, sz.height() - thick, len, thick);
    }
    return {};
}

void EdgeTab::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setPen(Qt::NoPen);

    // Franja casi invisible (alfa mínimo) para que Windows detecte el mouse ahí.
    p.setBrush(QColor(0, 0, 0, 1));
    p.drawRect(edgeRect(m_edge, size(), kHitThick, kHoverLen));

    const qreal thick = kRestThick + (kHoverThick - kRestThick) * m_grow;
    const qreal len   = kRestLen + (kHoverLen - kRestLen) * m_grow;
    QColor c = QColor(0x70, 0x70, 0x70);
    c.setRgbF(c.redF() + (0.80 - c.redF()) * m_grow, c.greenF() + (0.80 - c.greenF()) * m_grow,
              c.blueF() + (0.80 - c.blueF()) * m_grow, 0.6 + 0.35 * m_grow);
    p.setBrush(c);
    const QRectF pill = edgeRect(m_edge, size(), thick, len);
    p.drawRoundedRect(pill, qMin(thick, len) / 2, qMin(thick, len) / 2);

    // Flechita hacia adentro cuando está desplegada.
    if (m_grow > 0.5) {
        QColor arrow(0x1c, 0x1c, 0x1c);
        arrow.setAlphaF((m_grow - 0.5) * 2);
        p.setPen(QPen(arrow, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF c0 = pill.center();
        const qreal a = 3.2;
        QPolygonF chevron;
        switch (m_edge) {
        case Edge::Left:   chevron << c0 + QPointF(-a / 2, -a) << c0 + QPointF(a / 2, 0) << c0 + QPointF(-a / 2, a); break;
        case Edge::Right:  chevron << c0 + QPointF(a / 2, -a) << c0 + QPointF(-a / 2, 0) << c0 + QPointF(a / 2, a); break;
        case Edge::Top:    chevron << c0 + QPointF(-a, -a / 2) << c0 + QPointF(0, a / 2) << c0 + QPointF(a, -a / 2); break;
        case Edge::Bottom: chevron << c0 + QPointF(-a, a / 2) << c0 + QPointF(0, -a / 2) << c0 + QPointF(a, a / 2); break;
        }
        p.drawPolyline(chevron);
    }
}

void EdgeTab::enterEvent(QEnterEvent*) { animateGrow(1.0); }
void EdgeTab::leaveEvent(QEvent*) { animateGrow(0.0); }

void EdgeTab::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) emit activated();
}
