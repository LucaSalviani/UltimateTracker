#include "ui/GridWidget.h"
#include "core/Format.h"
#include "ui/Theme.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <cmath>

namespace {
const char* kWeekdays[] = {"lun", "mar", "mié", "jue", "vie", "sáb", "dom"};
}

GridWidget::GridWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_today = m_anchor = QDate::currentDate();
}

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
void GridWidget::setZoom(Zoom z) { m_zoom = z; update(); }
void GridWidget::setAnchor(const QDate& d) { m_anchor = d; update(); }
void GridWidget::setToday(const QDate& d) { m_today = d; update(); }
void GridWidget::setProportional(bool on) { m_proportional = on; update(); }

QDate GridWidget::rangeStart() const
{
    switch (m_zoom) {
    case Zoom::Day:  return m_anchor;
    case Zoom::Week: return fmt::mondayOf(m_anchor);
    case Zoom::Month:
    default:         return fmt::mondayOf(QDate(m_anchor.year(), m_anchor.month(), 1));
    }
}

QDate GridWidget::rangeEnd() const
{
    switch (m_zoom) {
    case Zoom::Day:  return m_anchor;
    case Zoom::Week: return fmt::mondayOf(m_anchor).addDays(6);
    case Zoom::Month:
    default: {
        const QDate last(m_anchor.year(), m_anchor.month(), m_anchor.daysInMonth());
        return fmt::mondayOf(last).addDays(6);
    }
    }
}

void GridWidget::setData(const QVector<Pomodoro>& pomos, const QHash<QDate, QString>& titles)
{
    m_pomos = pomos;
    m_titles = titles;
    m_byDay.clear();
    for (int i = 0; i < m_pomos.size(); ++i)
        m_byDay[m_pomos[i].day].append(i);
    recomputeDayEnds();
    update();
}

void GridWidget::recomputeDayEnds()
{
    m_lastOfDay.clear();
    for (auto it = m_byDay.cbegin(); it != m_byDay.cend(); ++it) {
        const Pomodoro* last = nullptr;
        for (int i : it.value())
            if (!last || m_pomos[i].startedAt >= last->startedAt) last = &m_pomos[i];
        if (last) m_lastOfDay.insert(it.key(), last->id);
    }
}

// Rojo = el pomodoro con el que terminó el día. Para hoy es "provisorio": se
// pinta rojo mientras no haya nada en marcha (si volvés a estudiar, pasa a negro).
bool GridWidget::isDayEnd(const Pomodoro& p) const
{
    if (p.status == PomoStatus::Running) return false;
    if (m_lastOfDay.value(p.day) != p.id) return false;
    return !(p.day == m_today && m_todayActive);
}

void GridWidget::setLive(const std::optional<Pomodoro>& running, double progress)
{
    m_liveId = running ? running->id : 0;
    m_liveProgress = progress;
    if (running) {
        for (Pomodoro& p : m_pomos)
            if (p.id == running->id) { p = *running; break; }
    }
    update();
}

qint64 GridWidget::studyOn(const QDate& d) const
{
    qint64 s = 0;
    for (int i : m_byDay.value(d)) s += m_pomos[i].studiedSec;
    return s;
}

// ---------------------------------------------------------------------------
// Acomodo de cuadraditos
// ---------------------------------------------------------------------------
// Flujo de izquierda a derecha con salto de línea. Se busca el tamaño más grande
// que entra en el área. En modo proporcional el lado es ∝ √minutos, así el ÁREA
// del cuadradito es proporcional al tiempo.
QVector<GridWidget::Placed> GridWidget::layoutSquares(const QRectF& area, const QVector<int>& indices,
                                                      double maxSide, double minSide) const
{
    QVector<Placed> out;
    if (indices.isEmpty() || area.width() <= 4 || area.height() <= 4) return out;

    QVector<double> w(indices.size(), 1.0);
    double maxW = 1.0;
    if (m_proportional) {
        for (int i = 0; i < indices.size(); ++i) {
            const double sec = qMax(60, m_pomos[indices[i]].representedSec());
            w[i] = std::sqrt(sec / 1500.0);   // 25' = tamaño base
            maxW = qMax(maxW, w[i]);
        }
    }

    auto attempt = [&](double k, QVector<Placed>* placed) -> bool {
        const double gap = qBound(2.0, k * 0.14, 7.0);
        double x = area.left(), y = area.top(), rowH = 0;
        int rowStart = 0;
        auto closeRow = [&](int end) {
            if (!placed) return;
            for (int j = rowStart; j < end; ++j) {
                QRectF& r = (*placed)[j].rect;
                r.moveBottom(y + rowH);  // alineados abajo: se lee como un gráfico
            }
        };
        for (int i = 0; i < indices.size(); ++i) {
            const double s = k * w[i];
            if (x + s > area.right() + 0.01 && x > area.left()) {
                closeRow(i);
                rowStart = i;
                x = area.left();
                y += rowH + gap;
                rowH = 0;
            }
            if (placed) placed->append({QRectF(x, y, s, s), indices[i]});
            x += s + gap;
            rowH = qMax(rowH, s);
        }
        closeRow(indices.size());
        return y + rowH <= area.bottom() + 0.01;
    };

    double hi = maxSide / maxW;
    double lo = minSide;
    double best = lo;
    if (attempt(hi, nullptr)) {
        best = hi;
    } else if (attempt(lo, nullptr)) {
        for (int it = 0; it < 24; ++it) {
            const double mid = (lo + hi) / 2;
            if (attempt(mid, nullptr)) lo = mid; else hi = mid;
        }
        best = lo;
    }
    attempt(best, &out);
    return out;
}

void GridWidget::paintSquare(QPainter& p, const QRectF& r, const Pomodoro& pomo) const
{
    const double s = r.width();
    const double radius = qMax(1.0, s * 0.12);
    QColor textColor;

    p.setPen(Qt::NoPen);
    const bool dayEnd = isDayEnd(pomo);
    switch (dayEnd ? PomoStatus::Final : (pomo.status == PomoStatus::Final ? PomoStatus::Completed : pomo.status)) {
    case PomoStatus::Completed:
        p.setBrush(theme::squareDone);
        p.setPen(QPen(theme::squareDoneEdge, 1));
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        textColor = theme::textDim;
        break;
    case PomoStatus::Final:
        p.setBrush(theme::squareFinal);
        p.drawRoundedRect(r, radius, radius);
        textColor = Qt::white;
        break;
    case PomoStatus::Running: {
        const double progress = pomo.id == m_liveId
            ? m_liveProgress
            : (pomo.plannedSec > 0 ? double(pomo.studiedSec) / pomo.plannedSec : 0.0);
        p.setBrush(theme::squareEmpty);
        p.setPen(QPen(theme::squareFill.darker(150), 1));
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        // Se llena de izquierda a derecha, como una barra que sigue a la anterior.
        p.save();
        QPainterPath clip;
        clip.addRoundedRect(r, radius, radius);
        p.setClipPath(clip);
        p.setPen(Qt::NoPen);
        p.setBrush(theme::squareFill);
        const double w = s * qBound(0.0, progress, 1.0);
        p.drawRect(QRectF(r.left(), r.top(), w, s));
        p.restore();
        textColor = theme::text;
        break;
    }
    }

    if (s < 15) return;
    const QString label = fmt::duration(pomo.representedSec());
    QFont f = theme::uiFont(9, QFont::DemiBold);
    double px = s * 0.30;
    f.setPixelSize(qMax(7, int(px)));
    QFontMetricsF fm(f);
    while (fm.horizontalAdvance(label) > s * 0.86 && px > 7) {
        px -= 0.5;
        f.setPixelSize(int(px));
        fm = QFontMetricsF(f);
    }
    p.setFont(f);
    p.setPen(textColor);
    p.drawText(r, Qt::AlignCenter, label);
}

void GridWidget::paintSquares(QPainter& p, const QRectF& area, const QDate& day, double maxSide, double minSide)
{
    const auto placed = layoutSquares(area, m_byDay.value(day), maxSide, minSide);
    p.save();
    p.setClipRect(area.adjusted(-1, -1, 1, 1));
    for (const Placed& pl : placed) {
        paintSquare(p, pl.rect, m_pomos[pl.index]);
        m_placed.append(pl);
    }
    p.restore();
}

// ---------------------------------------------------------------------------
// Vistas
// ---------------------------------------------------------------------------
void GridWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), theme::bg);
    m_placed.clear();
    m_dayRects.clear();

    switch (m_zoom) {
    case Zoom::Day:   paintDay(p); break;
    case Zoom::Week:  paintWeek(p); break;
    case Zoom::Month: paintMonth(p); break;
    }
}

void GridWidget::paintDay(QPainter& p)
{
    const QRectF area = QRectF(rect()).adjusted(14, 10, -14, -8);
    if (!m_byDay.contains(m_anchor)) {
        p.setPen(theme::textFaint);
        p.setFont(theme::uiFont(9));
        p.drawText(area, Qt::AlignCenter,
                   m_anchor == m_today ? QStringLiteral("Dale play para sumar el primer cuadradito")
                                       : QStringLiteral("Sin pomodoros este día"));
        return;
    }
    paintSquares(p, area, m_anchor, 58, 10);
}

void GridWidget::paintWeek(QPainter& p)
{
    const QRectF all = QRectF(rect()).adjusted(8, 4, -8, -6);
    const double colW = all.width() / 7.0;
    const double headerH = 48;
    const QDate monday = fmt::mondayOf(m_anchor);

    for (int i = 0; i < 7; ++i) {
        const QDate d = monday.addDays(i);
        const QRectF col(all.left() + i * colW, all.top(), colW, all.height());
        m_dayRects.append({col, d});
        const bool isToday = d == m_today;

        QRectF header = col.adjusted(3, 0, -3, 0);
        header.setHeight(headerH);
        p.setPen(Qt::NoPen);
        p.setBrush(isToday ? theme::surfaceHi : theme::surface);
        p.drawRoundedRect(header, 6, 6);

        p.setFont(theme::uiFont(9.5, isToday ? QFont::DemiBold : QFont::Normal));
        p.setPen(isToday ? theme::text : theme::textDim);
        p.drawText(header.adjusted(8, 5, -8, 0), Qt::AlignLeft | Qt::AlignTop, fmt::shortDate(d));

        const qint64 study = studyOn(d);
        if (study > 0) {
            p.setFont(theme::uiFont(8.5));
            p.setPen(theme::textDim);
            p.drawText(header.adjusted(8, 5, -8, 0), Qt::AlignRight | Qt::AlignTop, fmt::duration(study));
        }
        const QString title = m_titles.value(d);
        if (!title.isEmpty()) {
            p.setFont(theme::uiFont(8));
            p.setPen(theme::textFaint);
            const QRectF tr = header.adjusted(8, 25, -8, -4);
            p.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(p.font()).elidedText(title, Qt::ElideRight, int(tr.width())));
        }

        if (i > 0) {
            p.setPen(QPen(theme::line, 1));
            p.drawLine(QPointF(col.left(), col.top() + headerH + 8), QPointF(col.left(), col.bottom()));
        }
        const QRectF area = col.adjusted(8, headerH + 10, -8, -2);
        paintSquares(p, area, d, 40, 6);
    }
}

void GridWidget::paintMonth(QPainter& p)
{
    const QRectF all = QRectF(rect()).adjusted(8, 2, -8, -6);
    const double headerH = 22;
    const QDate first = rangeStart();
    const int weeks = int(first.daysTo(rangeEnd()) + 1) / 7;
    const double colW = all.width() / 7.0;
    const double rowH = (all.height() - headerH) / weeks;

    p.setFont(theme::uiFont(8.5));
    p.setPen(theme::textFaint);
    for (int c = 0; c < 7; ++c)
        p.drawText(QRectF(all.left() + c * colW + 8, all.top(), colW, headerH),
                   Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8(kWeekdays[c]));

    for (int r = 0; r < weeks; ++r) {
        for (int c = 0; c < 7; ++c) {
            const QDate d = first.addDays(r * 7 + c);
            const QRectF cell(all.left() + c * colW, all.top() + headerH + r * rowH, colW, rowH);
            const QRectF box = cell.adjusted(2, 2, -2, -2);
            m_dayRects.append({cell, d});
            const bool inMonth = d.month() == m_anchor.month();
            const bool isToday = d == m_today;

            p.setPen(Qt::NoPen);
            p.setBrush(isToday ? theme::surfaceHi : (inMonth ? theme::surface : theme::bg));
            p.drawRoundedRect(box, 6, 6);

            p.setFont(theme::uiFont(8.5, isToday ? QFont::Bold : QFont::Normal));
            p.setPen(isToday ? theme::text : (inMonth ? theme::textDim : theme::textFaint));
            p.drawText(box.adjusted(7, 4, -7, 0), Qt::AlignLeft | Qt::AlignTop, QString::number(d.day()));

            const qint64 study = studyOn(d);
            if (study > 0) {
                p.setFont(theme::uiFont(8));
                p.setPen(theme::textDim);
                p.drawText(box.adjusted(7, 4, -7, 0), Qt::AlignRight | Qt::AlignTop, fmt::duration(study));
            }
            const QRectF area = box.adjusted(6, 21, -6, -5);
            paintSquares(p, area, d, 34, 3);
        }
    }
}

// ---------------------------------------------------------------------------
// Interacción
// ---------------------------------------------------------------------------
void GridWidget::mouseMoveEvent(QMouseEvent* e)
{
    for (const Placed& pl : m_placed) {
        if (!pl.rect.contains(e->position())) continue;
        const Pomodoro& pm = m_pomos[pl.index];
        const QString start = QDateTime::fromSecsSinceEpoch(pm.startedAt).time().toString("HH:mm");
        QString status;
        if (pm.status == PomoStatus::Running)       status = QStringLiteral("en curso");
        else if (pm.studiedSec >= pm.plannedSec)    status = QStringLiteral("completo");
        else                                        status = QStringLiteral("cortado antes");
        if (isDayEnd(pm)) status += QStringLiteral(" · último del día");
        QToolTip::showText(e->globalPosition().toPoint(),
                           QString("%1\n%2 · %3 de %4 · %5")
                               .arg(m_paths.value(pm.folderId, QStringLiteral("Sin actividad")), start,
                                    fmt::duration(pm.studiedSec), fmt::duration(pm.plannedSec), status),
                           this);
        return;
    }
    QToolTip::hideText();
}

void GridWidget::contextMenuEvent(QContextMenuEvent* e)
{
    for (const Placed& pl : m_placed) {
        if (!pl.rect.contains(e->pos())) continue;
        const Pomodoro& pm = m_pomos[pl.index];
        QMenu menu(this);
        QAction* del = menu.addAction(QStringLiteral("Borrar pomodoro…"));
        if (pm.status == PomoStatus::Running) {
            del->setEnabled(false);
            del->setText(QStringLiteral("Borrar pomodoro (terminalo primero)"));
        }
        const qint64 id = pm.id;
        if (menu.exec(e->globalPos()) == del) emit deleteRequested(id);
        return;
    }
}

void GridWidget::mousePressEvent(QMouseEvent* e)
{
    if (m_zoom == Zoom::Day || e->button() != Qt::LeftButton) return;
    for (const auto& [r, d] : m_dayRects) {
        if (r.contains(e->position())) {
            emit dayActivated(d);
            return;
        }
    }
}
