#include "ui/LiveStats.h"
#include "core/Format.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
QFont condensed(qreal px, int weight)
{
    QFont f;
    f.setFamilies({QStringLiteral("Bahnschrift"), QStringLiteral("DIN Alternate"), QStringLiteral("Inter"),
                   QStringLiteral("Segoe UI"), QStringLiteral("sans-serif")});
    f.setPixelSize(int(px));
    f.setWeight(QFont::Weight(weight));
    f.setStretch(QFont::SemiCondensed);
    return f;
}
} // namespace

LiveStats::LiveStats(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFixedHeight(64);
}

void LiveStats::setData(const Data& d)
{
    m_data = d;
    setToolTip(d.weekFrac >= 0 ? d.weekText : QString());
    update();
}

QRectF LiveStats::toggleRect() const
{
    return QRectF(width() - 150, 2, 148, 16);
}

void LiveStats::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Cabecera: alcance + toggle
    const QString scope = m_data.activityScope
        ? QStringLiteral("HOY · %1").arg(m_data.scopeName.toUpper())
        : QStringLiteral("HOY · TODO EL DÍA");
    QFont hf = condensed(11, QFont::DemiBold);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    p.setFont(hf);
    p.setPen(theme::textDim);
    const QRectF tr = toggleRect();
    p.drawText(QRectF(0, 2, tr.left() - 8, 16), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(hf).elidedText(scope, Qt::ElideRight, int(tr.left() - 8)));

    QFont tf = condensed(11, QFont::Normal);
    p.setFont(tf);
    p.setPen(m_hoverToggle ? theme::text : theme::textFaint);
    p.drawText(tr, Qt::AlignRight | Qt::AlignVCenter,
               m_data.activityScope ? QStringLiteral("ver todo el día ⇄") : QStringLiteral("ver esta actividad ⇄"));

    // Tres bloques: número grande + etiqueta
    struct Block { QString big; QString small; };
    // M32: un solo formato de tiempo ("1h40") y "pomodoros" con todas las letras.
    const Block blocks[3] = {
        {QString::number(m_data.pomos), m_data.pomos == 1 ? QStringLiteral("pomodoro") : QStringLiteral("pomodoros")},
        {fmt::duration(m_data.seconds), QStringLiteral("estudiado")},
        m_data.activityScope
            ? Block{fmt::duration(m_data.activityTotalSec), QStringLiteral("en total")}
            : Block{QString::number(m_data.activities),
                    m_data.activities == 1 ? QStringLiteral("actividad") : QStringLiteral("actividades")},
    };
    const qreal top = 22, h = height() - top;
    const qreal w = width() / 3.0;
    for (int i = 0; i < 3; ++i) {
        const QRectF cell(i * w, top, w, h);
        if (i > 0) {
            p.setPen(QPen(theme::line, 1));
            p.drawLine(QPointF(cell.left(), cell.top() + 6), QPointF(cell.left(), cell.bottom() - 4));
        }
        const qreal x = cell.left() + (i == 0 ? 0 : 12);
        QFont bf = condensed(26, QFont::Bold);
        p.setFont(bf);
        p.setPen(i == 0 ? theme::pomoAccent : theme::text);
        const qreal bw = QFontMetricsF(bf).horizontalAdvance(blocks[i].big);
        p.drawText(QRectF(x, cell.top(), bw + 2, h), Qt::AlignLeft | Qt::AlignVCenter, blocks[i].big);
        QFont sf = condensed(11, QFont::Medium);
        p.setFont(sf);
        p.setPen(theme::textDim);
        const QString small = QFontMetrics(sf).elidedText(blocks[i].small, Qt::ElideRight, int(cell.right() - x - bw - 8));
        p.drawText(QRectF(x + bw + 6, cell.top() + 4, cell.right() - x - bw - 8, h - 4),
                   Qt::AlignLeft | Qt::AlignVCenter, small);
        // M44: marca del día: tres puntitos (bronce, plata, oro) al lado de "estudiado".
        if (i == 1 && m_data.dayLevel >= 0) {
            static const QColor medal[3] = {QColor(0xc0, 0x8a, 0x5a), QColor(0xb8, 0xbc, 0xc4), QColor(0xe0, 0xb8, 0x4a)};
            const double dx = x + bw + 6 + QFontMetricsF(sf).horizontalAdvance(small) + 8;
            const double cy = cell.top() + h / 2 + 2;
            for (int k = 0; k < 3; ++k) {
                const QRectF dot(dx + k * 9, cy - 3, 6, 6);
                if (dot.right() > cell.right() - 4) break;
                if (k < m_data.dayLevel) { p.setPen(Qt::NoPen); p.setBrush(medal[k]); }
                else { p.setPen(QPen(theme::textFaint, 1)); p.setBrush(Qt::NoBrush); }
                p.drawEllipse(dot);
            }
        }
    }
    // M44: meta semanal: una línea finita abajo de todo que se va llenando.
    if (m_data.weekFrac >= 0) {
        const QRectF track(0, height() - 2.5, width(), 2);
        p.setPen(Qt::NoPen);
        p.setBrush(theme::line);
        p.drawRoundedRect(track, 1, 1);
        p.setBrush(m_data.weekFrac >= 1 ? theme::breakAccent : theme::pomoAccent);
        p.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * qMin(1.0, m_data.weekFrac), 2), 1, 1);
    }
}

void LiveStats::mouseMoveEvent(QMouseEvent* e)
{
    const bool over = toggleRect().contains(e->position());
    if (over != m_hoverToggle) {
        m_hoverToggle = over;
        setCursor(over ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void LiveStats::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && toggleRect().contains(e->position())) emit scopeToggled();
    else e->ignore();
}

void LiveStats::leaveEvent(QEvent*)
{
    m_hoverToggle = false;
    update();
}
