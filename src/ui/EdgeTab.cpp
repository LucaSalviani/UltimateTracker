#include "ui/EdgeTab.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr int kThickness = 10;  // área clickeable
constexpr int kLength    = 84;
}

EdgeTab::EdgeTab(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                          Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Abrir Constancia"));
}

void EdgeTab::attach(Edge edge, const QRect& s, int along)
{
    m_edge = edge;
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

void EdgeTab::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor c = m_hover ? QColor(0xb0, 0xb0, 0xb0) : QColor(0x70, 0x70, 0x70);
    c.setAlphaF(m_hover ? 0.95 : 0.6);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    // Píldora finita pegada al borde (el resto del área es invisible pero clickeable).
    QRectF pill;
    const qreal t = 5;
    switch (m_edge) {
    case Edge::Left:   pill = QRectF(0, 0, t, height()); break;
    case Edge::Right:  pill = QRectF(width() - t, 0, t, height()); break;
    case Edge::Top:    pill = QRectF(0, 0, width(), t); break;
    case Edge::Bottom: pill = QRectF(0, height() - t, width(), t); break;
    }
    p.drawRoundedRect(pill, t / 2, t / 2);
}

void EdgeTab::enterEvent(QEnterEvent*) { m_hover = true; update(); }
void EdgeTab::leaveEvent(QEvent*) { m_hover = false; update(); }

void EdgeTab::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) emit activated();
}
