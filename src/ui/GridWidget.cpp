#include "ui/GridWidget.h"
#include "core/DayClock.h"
#include "core/Format.h"
#include "ui/Theme.h"

#include <QContextMenuEvent>
#include <QEasingCurve>
#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <cmath>

namespace {
const char* kWeekdays[] = {"lun", "mar", "mié", "jue", "vie", "sáb", "dom"};
constexpr double kLinkGap = 2.0;   // separación entre pomodoros encadenados
}

GridWidget::GridWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_today = m_anchor = dayclock::today();
    m_clock.start();
    m_animTimer.setInterval(16);
    connect(&m_animTimer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
}

void GridWidget::setFolderLooks(const QHash<qint64, QString>& names, const QHash<qint64, QColor>& colors)
{
    m_names = names;
    m_colors = colors;
    update();
}

// ---------------------------------------------------------------------------
// Animación de entrada (M8): al cambiar de vista los cuadraditos aparecen en
// cascada con un pequeño rebote; uno nuevo "salta" solo.
// ---------------------------------------------------------------------------
void GridWidget::startIntro()
{
    m_introAt = m_clock.elapsed();
    m_popAt.clear();
}

bool GridWidget::animFrame(qint64 id, int order, double* scale, double* opacity)
{
    const qint64 now = m_clock.elapsed();
    qint64 start = -1;
    if (m_introAt >= 0) start = m_introAt + qMin(order * 16, 520);
    else if (m_popAt.contains(id)) start = m_popAt.value(id);
    *scale = 1.0;
    *opacity = 1.0;
    if (start < 0) return false;
    const double t = double(now - start) / 360.0;
    if (t >= 1.0) return false;
    if (t <= 0.0) { *scale = 0.0; *opacity = 0.0; return true; }
    *scale = QEasingCurve(QEasingCurve::OutBack).valueForProgress(t);
    *opacity = qMin(1.0, t * 2.2);
    return true;
}

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
void GridWidget::setZoom(Zoom z)
{
    if (z != m_zoom) startIntro();
    m_zoom = z;
    m_hoverDay = {};
    unsetCursor();
    update();
}
void GridWidget::setAnchor(const QDate& d)
{
    if (d != m_anchor) startIntro();
    m_anchor = d;
    update();
}
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
    m_real = pomos;
    m_titles = titles;
    rebuild();
}

void GridWidget::setGoalBlocks(const QHash<QDate, QVector<int>>& blocks)
{
    m_goalBlocks = blocks;
    rebuild();
}

void GridWidget::rebuild()
{
    m_pomos = m_real;
    m_byDay.clear();
    for (int i = 0; i < m_pomos.size(); ++i)
        m_byDay[m_pomos[i].day].append(i);
    // Fantasmas: los cuadraditos de la meta que faltan, a continuación de los reales.
    // (MainWindow ya descontó los hechos: acá llegan solo los que faltan.)
    for (auto it = m_goalBlocks.cbegin(); it != m_goalBlocks.cend(); ++it) {
        for (int k = 0; k < it.value().size(); ++k) {
            Pomodoro g;
            g.id = -(it.key().toJulianDay() * 100 + k + 1);   // estable entre recargas
            g.day = it.key();
            g.plannedSec = g.studiedSec = it.value()[k] * 60;
            g.status = PomoStatus::Completed;
            m_byDay[g.day].append(int(m_pomos.size()));
            m_pomos.append(g);
        }
    }
    recomputeDayEnds();
    // Lo que aparece por primera vez "salta" (si no hay una entrada en curso).
    const qint64 now = m_clock.elapsed();
    if (m_seen.isEmpty() && m_introAt < 0) startIntro();
    for (const Pomodoro& p : m_pomos) {
        if (m_seen.contains(p.id)) continue;
        if (m_introAt < 0) m_popAt.insert(p.id, now);
        m_seen.insert(p.id);
    }
    update();
}

void GridWidget::recomputeDayEnds()
{
    m_lastOfDay.clear();
    m_number.clear();
    for (auto it = m_byDay.cbegin(); it != m_byDay.cend(); ++it) {
        // m_pomos viene ordenado por inicio: el orden de la lista es la numeración.
        int n = 0;
        const Pomodoro* last = nullptr;
        for (int i : it.value()) {
            if (isGhost(m_pomos[i])) continue;
            m_number.insert(m_pomos[i].id, ++n);
            if (!last || m_pomos[i].startedAt >= last->startedAt) last = &m_pomos[i];
        }
        if (last) m_lastOfDay.insert(it.key(), last->id);
    }
}

// Rojo = el pomodoro con el que terminó el día. Para hoy es "provisorio": se
// pinta rojo mientras no haya nada en marcha (si volvés a estudiar, pasa a negro).
bool GridWidget::isDayEnd(const Pomodoro& p) const
{
    if (p.status == PomoStatus::Running || p.status == PomoStatus::Parked) return false;
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
    for (int i : m_byDay.value(d)) if (!isGhost(m_pomos[i])) s += m_pomos[i].studiedSec;
    return s;
}

// ---------------------------------------------------------------------------
// Acomodo de cuadraditos
// ---------------------------------------------------------------------------
// Flujo de izquierda a derecha con salto de línea. Se busca el tamaño más grande
// que entra en el área. En modo proporcional el lado es ∝ √minutos, así el ÁREA
// del cuadradito es proporcional al tiempo. Con grupos, cada actividad arranca
// en su renglón con su título arriba.
GridWidget::Layout GridWidget::layoutGroups(const QRectF& area, const QVector<QVector<int>>& groups,
                                            double headerH, bool breakLines, double maxSide, double minSide) const
{
    Layout out;
    int count = 0;
    for (const auto& g : groups) count += int(g.size());
    if (count == 0 || area.width() <= 4 || area.height() <= 4) return out;

    QHash<int, double> w;
    double maxW = 1.0;
    for (const auto& g : groups)
        for (int idx : g) {
            double v = 1.0;
            if (m_proportional) v = std::sqrt(qMax(60, m_pomos[idx].representedSec()) / 1500.0);   // 25' = base
            w.insert(idx, v);
            maxW = qMax(maxW, v);
        }

    auto attempt = [&](double k, Layout* lay) -> bool {
        const double gap = qBound(2.0, k * 0.14, 7.0);
        double x = area.left(), y = area.top(), rowH = 0;
        int rowStart = 0;
        auto closeRow = [&]() {
            if (!lay) return;
            for (int j = rowStart; j < lay->placed.size(); ++j) {
                QRectF& r = lay->placed[j].rect;
                r.moveBottom(y + rowH);   // alineados abajo: se lee como un gráfico
            }
            rowStart = lay ? int(lay->placed.size()) : 0;
        };
        bool firstGroup = true;
        for (const auto& g : groups) {
            if (g.isEmpty()) continue;
            if (breakLines && !firstGroup) {
                closeRow();
                y += rowH + gap + (headerH > 0 ? gap * 0.6 : 0);
                x = area.left();
                rowH = 0;
            }
            if (breakLines && headerH > 0) {
                if (lay) {
                    Header h{QRectF(area.left(), y, area.width(), headerH), m_pomos[g.first()].folderId, 0, 0};
                    for (int idx : g) {
                        if (isGhost(m_pomos[idx])) ++h.ghosts; else h.seconds += m_pomos[idx].studiedSec;
                    }
                    lay->headers.append(h);
                }
                y += headerH;
            }
            firstGroup = false;
            for (int i = 0; i < g.size(); ++i) {
                const int idx = g[i];
                const double s = k * w.value(idx);
                // Un pomodoro encadenado (sin recreo) va pegado al anterior.
                if (i > 0 && m_pomos[idx].linked && x > area.left()) x -= gap - kLinkGap;
                if (x + s > area.right() + 0.01 && x > area.left()) {
                    closeRow();
                    x = area.left();
                    y += rowH + gap;
                    rowH = 0;
                }
                if (lay) lay->placed.append({QRectF(x, y, s, s), idx});
                x += s + gap;
                rowH = qMax(rowH, s);
            }
        }
        closeRow();
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

// Fantasma: el lugar de un pomodoro de la meta que todavía no hiciste.
void GridWidget::paintGhost(QPainter& p, const QRectF& r, const Pomodoro& g) const
{
    const double s = r.width();
    const double radius = qMax(1.0, s * 0.12);
    QColor edge = theme::textFaint;
    edge.setAlphaF(0.7);
    QPen pen(edge, 1, Qt::DashLine);
    pen.setDashPattern({3, 3});
    p.setPen(pen);
    QColor fill = theme::text;
    fill.setAlphaF(0.025);
    p.setBrush(fill);
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    if (s < 15) return;
    QFont f = theme::uiFont(9, QFont::Medium);
    f.setPixelSize(qMax(7, int(s * 0.26)));
    p.setFont(f);
    p.setPen(theme::textFaint);
    p.drawText(r, Qt::AlignCenter, fmt::duration(g.plannedSec));
}

void GridWidget::paintSquare(QPainter& p, const QRectF& r, const Pomodoro& pomo) const
{
    if (isGhost(pomo)) { paintGhost(p, r, pomo); return; }
    const double s = r.width();
    const double radius = qMax(1.0, s * 0.12);
    QColor textColor;

    p.setPen(Qt::NoPen);
    const bool dayEnd = isDayEnd(pomo);
    switch (dayEnd ? PomoStatus::Final : (pomo.status == PomoStatus::Final ? PomoStatus::Completed : pomo.status)) {
    case PomoStatus::Completed: {
        // Sombra apenas debajo + cuerpo con un brillo arriba, como una tecla.
        if (s >= 12) {
            p.setBrush(QColor(0, 0, 0, 70));
            p.drawRoundedRect(r.translated(0, qMax(1.0, s * 0.035)), radius, radius);
        }
        QLinearGradient body(r.topLeft(), r.bottomLeft());
        body.setColorAt(0, theme::mix(theme::squareDone, theme::squareDoneEdge, 0.30));
        body.setColorAt(0.45, theme::squareDone);
        body.setColorAt(1, theme::squareDone);
        p.setBrush(body);
        p.setPen(QPen(theme::squareDoneEdge, 1));
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        textColor = theme::mix(theme::squareDone, theme::squareDone.lightness() < 128 ? Qt::white : Qt::black, 0.6);
        break;
    }
    case PomoStatus::Final: {
        if (s >= 12) {
            p.setBrush(QColor(0, 0, 0, 70));
            p.drawRoundedRect(r.translated(0, qMax(1.0, s * 0.035)), radius, radius);
        }
        QLinearGradient body(r.topLeft(), r.bottomLeft());
        body.setColorAt(0, theme::squareFinal.lighter(122));
        body.setColorAt(1, theme::squareFinal.darker(108));
        p.setBrush(body);
        p.drawRoundedRect(r, radius, radius);
        textColor = Qt::white;
        break;
    }
    case PomoStatus::Running:
    case PomoStatus::Parked: {
        const bool parked = pomo.status == PomoStatus::Parked;
        const double progress = pomo.id == m_liveId
            ? m_liveProgress
            : (pomo.plannedSec > 0 ? double(pomo.studiedSec) / pomo.plannedSec : 0.0);
        p.setBrush(theme::squareEmpty);
        // En pausa por cambio de actividad: borde punteado.
        p.setPen(QPen(theme::squareFill.darker(150), 1, parked ? Qt::DashLine : Qt::SolidLine));
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        // Se llena de izquierda a derecha, como una barra que sigue a la anterior.
        p.save();
        QPainterPath clip;
        clip.addRoundedRect(r, radius, radius);
        p.setClipPath(clip);
        p.setPen(Qt::NoPen);
        p.setBrush(parked ? theme::squareFill.darker(160) : theme::squareFill);
        const double w = s * qBound(0.0, progress, 1.0);
        p.drawRect(QRectF(r.left(), r.top(), w, s));
        p.restore();
        if (parked && s >= 22) {
            // ⏸ chiquito arriba a la derecha
            const double g = s * 0.16, bw = g * 0.32;
            const QPointF o(r.right() - s * 0.1 - g, r.top() + s * 0.1);
            p.setPen(Qt::NoPen);
            p.setBrush(theme::text);
            p.drawRect(QRectF(o.x(), o.y(), bw, g));
            p.drawRect(QRectF(o.x() + g * 0.62, o.y(), bw, g));
        }
        textColor = parked ? theme::textDim : theme::text;
        break;
    }
    }

    // Rayita del color de la actividad abajo (M8) + filo de luz arriba.
    if (s >= 12) {
        p.save();
        QPainterPath clip;
        clip.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        p.setClipPath(clip);
        p.setPen(Qt::NoPen);
        const QColor c = m_colors.value(pomo.folderId);
        if (c.isValid()) {
            QColor stripe = c;
            stripe.setAlphaF(dayEnd ? 0.55 : 0.9);
            p.setBrush(stripe);
            p.drawRect(QRectF(r.left(), r.bottom() - qMax(2.0, s * 0.075), s, qMax(2.0, s * 0.075)));
        }
        QColor hl(255, 255, 255, dayEnd ? 60 : 22);
        p.setBrush(hl);
        p.drawRect(QRectF(r.left(), r.top(), s, 1.0));
        p.restore();
    }

    if (s < 15) return;
    if (s >= 30) {
        const int n = m_number.value(pomo.id);
        QFont nf = theme::uiFont(8, QFont::DemiBold);
        nf.setPixelSize(qMax(8, int(s * 0.17)));
        p.setFont(nf);
        QColor nc = textColor;
        nc.setAlphaF(0.55);
        p.setPen(nc);
        p.drawText(r.adjusted(s * 0.09, s * 0.05, 0, 0), Qt::AlignLeft | Qt::AlignTop, QString::number(n));
    }
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

void GridWidget::paintSquares(QPainter& p, const QRectF& area, const QDate& day, double maxSide, double minSide,
                              double headerH, bool breakLines)
{
    // Grupos: por actividad en el orden en que aparecen; los fantasmas de la meta al final.
    const QVector<int> all = m_byDay.value(day);
    QVector<QVector<int>> groups;
    QVector<int> ghosts;
    if (m_grouping) {
        QHash<qint64, int> slot;
        for (int idx : all) {
            const Pomodoro& pm = m_pomos[idx];
            if (isGhost(pm)) { ghosts.append(idx); continue; }
            auto it = slot.find(pm.folderId);
            if (it == slot.end()) {
                it = slot.insert(pm.folderId, int(groups.size()));
                groups.append(QVector<int>());
            }
            groups[*it].append(idx);
        }
    } else {
        groups.append(all);
    }
    // Con una sola actividad no hace falta título: los fantasmas siguen en el mismo flujo.
    const bool titled = m_grouping && groups.size() >= 2;
    if (!ghosts.isEmpty()) {
        if (titled) groups.append(ghosts);
        else if (groups.isEmpty()) groups.append(ghosts);
        else groups.last() += ghosts;
    }
    const Layout lay = layoutGroups(area, groups, titled ? headerH : 0, breakLines && titled, maxSide, minSide);

    p.save();
    p.setClipRect(area.adjusted(-1, -1, 1, 1));
    for (const Header& h : lay.headers) paintHeader(p, h);
    // "Súper pomodoro": un puente une a los que se hicieron sin recreo.
    for (int k = 1; k < lay.placed.size(); ++k) {
        const QRectF& a = lay.placed[k - 1].rect;
        const QRectF& b = lay.placed[k].rect;
        const Pomodoro& pb = m_pomos[lay.placed[k].index];
        if (!pb.linked || isGhost(pb) || m_pomos[lay.placed[k - 1].index].folderId != pb.folderId ||
            qAbs(a.bottom() - b.bottom()) > 0.5 || b.left() < a.right()) continue;
        const double h = qMin(a.height(), b.height()) * 0.56;
        const double cy = b.bottom() - qMin(a.height(), b.height()) / 2;
        p.setPen(Qt::NoPen);
        p.setBrush(theme::bridge);
        p.drawRect(QRectF(a.right() - 3, cy - h / 2, b.left() - a.right() + 6, h));
    }
    for (const Placed& pl : lay.placed) {
        const Pomodoro& pm = m_pomos[pl.index];
        double scale = 1, opacity = 1;
        if (animFrame(pm.id, m_paintOrder++, &scale, &opacity)) m_animating = true;
        if (scale <= 0.01) { m_placed.append(pl); continue; }
        if (scale != 1.0 || opacity != 1.0) {
            p.save();
            p.setOpacity(opacity);
            const QPointF c = pl.rect.center();
            p.translate(c);
            p.scale(scale, scale);
            p.translate(-c);
            paintSquare(p, pl.rect, pm);
            p.restore();
        } else {
            paintSquare(p, pl.rect, pm);
        }
        m_placed.append(pl);
    }
    p.restore();
}

// Título de un grupo: ● Sistemas Operativos  1h40
void GridWidget::paintHeader(QPainter& p, const Header& h) const
{
    const bool ghost = h.ghosts > 0 && h.seconds == 0;
    const double fs = qBound(7.5, h.rect.height() * 0.50, 9.0);
    QFont f = theme::uiFont(fs, QFont::DemiBold);
    p.setFont(f);
    const QRectF r = h.rect.adjusted(0, 0, 0, -3);
    double x = r.left();
    if (!ghost) {
        const QColor c = m_colors.value(h.folderId, theme::textFaint);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const double d = qMax(5.0, fs * 0.7);
        p.drawEllipse(QRectF(x, r.center().y() - d / 2 + 1, d, d));
        x += d + 5;
    }
    QString name = ghost ? QStringLiteral("Objetivo · faltan %1").arg(h.ghosts)
                         : m_names.value(h.folderId, m_paths.value(h.folderId, QStringLiteral("Sin actividad")));
    const QString dur = ghost ? QString() : fmt::duration(int(h.seconds));
    QFontMetricsF fm(f);
    const double durW = dur.isEmpty() ? 0 : fm.horizontalAdvance(dur) + 6;
    name = fm.elidedText(name, Qt::ElideRight, qMax(10.0, r.right() - x - durW));
    p.setPen(ghost ? theme::textFaint : theme::textDim);
    p.drawText(QRectF(x, r.top(), r.right() - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter, name);
    if (!dur.isEmpty()) {
        p.setFont(theme::uiFont(fs, QFont::Normal));
        p.setPen(theme::textFaint);
        p.drawText(QRectF(x + fm.horizontalAdvance(name) + 6, r.top(), durW, r.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, dur);
    }
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
    m_paintOrder = 0;
    m_animating = false;

    switch (m_zoom) {
    case Zoom::Day:   paintDay(p); break;
    case Zoom::Week:  paintWeek(p); break;
    case Zoom::Month: paintMonth(p); break;
    }

    if (m_animating && !m_animTimer.isActive()) m_animTimer.start();
    if (!m_animating) {
        m_animTimer.stop();
        m_introAt = -1;
        m_popAt.clear();
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
    paintSquares(p, area, m_anchor, 58, 10, 20, true);
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
        const bool hover = d == m_hoverDay;
        if (hover) {
            // Toda la columna se ilumina: se nota que se puede clickear.
            p.setPen(Qt::NoPen);
            p.setBrush(theme::mix(theme::bg, theme::text, 0.03));
            p.drawRoundedRect(col.adjusted(1, 0, -1, 0), 8, 8);
        }
        p.setPen(hover ? QPen(theme::hoverEdge, 1) : QPen(Qt::NoPen));
        p.setBrush(hover ? theme::hover : (isToday ? theme::surfaceHi : theme::surface));
        p.drawRoundedRect(header.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);

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
        if (hover) {
            p.setFont(theme::uiFont(8, QFont::DemiBold));
            p.setPen(theme::text);
            p.drawText(header.adjusted(8, 25, -8, -4), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("ver día ›"));
        }
        if (!title.isEmpty()) {
            p.setFont(theme::uiFont(8));
            p.setPen(theme::textFaint);
            const QRectF tr = header.adjusted(8, 25, hover ? -62 : -8, -4);
            p.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(p.font()).elidedText(title, Qt::ElideRight, int(tr.width())));
        }

        if (i > 0) {
            p.setPen(QPen(theme::line, 1));
            p.drawLine(QPointF(col.left(), col.top() + headerH + 8), QPointF(col.left(), col.bottom()));
        }
        const QRectF area = col.adjusted(8, headerH + 10, -8, -2);
        paintSquares(p, area, d, 40, 6, area.width() >= 90 ? 15 : 0, true);
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

            const bool hover = d == m_hoverDay;
            p.setPen(hover ? QPen(theme::hoverEdge, 1) : QPen(Qt::NoPen));
            p.setBrush(hover ? theme::hover
                             : (isToday ? theme::surfaceHi : (inMonth ? theme::surface : theme::bg)));
            p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
            if (hover) {
                p.setFont(theme::uiFont(8, QFont::DemiBold));
                p.setPen(theme::text);
                p.drawText(box.adjusted(7, 0, -7, -4), Qt::AlignRight | Qt::AlignBottom, QStringLiteral("ver día ›"));
            }

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
            paintSquares(p, area, d, 34, 3, 0, false);
        }
    }
}

// ---------------------------------------------------------------------------
// Interacción
// ---------------------------------------------------------------------------
void GridWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (m_zoom != Zoom::Day) {
        QDate over;
        for (const auto& [r, d] : m_dayRects)
            if (r.contains(e->position())) { over = d; break; }
        if (over != m_hoverDay) {
            m_hoverDay = over;
            if (over.isValid()) setCursor(Qt::PointingHandCursor);
            else unsetCursor();
            update();
        }
    }
    if (m_zoom == Zoom::Day) unsetCursor();
    for (const Placed& pl : m_placed) {
        if (!pl.rect.contains(e->position())) continue;
        const Pomodoro& pm = m_pomos[pl.index];
        if (isGhost(pm)) {
            QToolTip::showText(e->globalPosition().toPoint(),
                               QStringLiteral("Objetivo: falta un pomodoro de %1\nClick para ver el objetivo del día")
                                   .arg(fmt::duration(pm.plannedSec)), this);
            setCursor(Qt::PointingHandCursor);
            return;
        }
        const QString start = QDateTime::fromSecsSinceEpoch(pm.startedAt).time().toString("HH:mm");
        QString status;
        if (pm.status == PomoStatus::Running)       status = QStringLiteral("en curso");
        else if (pm.status == PomoStatus::Parked)   status = QStringLiteral("en pausa (cambiaste de actividad)");
        else if (pm.studiedSec >= pm.plannedSec)    status = QStringLiteral("completo");
        else                                        status = QStringLiteral("cortado antes");
        if (isDayEnd(pm)) status += QStringLiteral(" · último del día");
        if (pm.linked) status += QStringLiteral(" · sin recreo antes");
        QString tip = QString("#%1  %2\n%3 · %4 de %5 · %6")
                          .arg(m_number.value(pm.id))
                          .arg(m_paths.value(pm.folderId, QStringLiteral("Sin actividad")), start,
                               fmt::duration(pm.studiedSec), fmt::duration(pm.plannedSec), status);
        if (!pm.note.isEmpty()) tip += QStringLiteral("\n✎ ") + pm.note;
        QToolTip::showText(e->globalPosition().toPoint(), tip, this);
        return;
    }
    QToolTip::hideText();
}

void GridWidget::leaveEvent(QEvent*)
{
    if (m_hoverDay.isValid()) {
        m_hoverDay = {};
        unsetCursor();
        update();
    }
}

QDate GridWidget::dayAt(const QPointF& pos) const
{
    if (m_zoom == Zoom::Day) return m_anchor;
    for (const auto& [r, d] : m_dayRects)
        if (r.contains(pos)) return d;
    return {};
}

void GridWidget::contextMenuEvent(QContextMenuEvent* e)
{
    for (const Placed& pl : m_placed) {
        if (!pl.rect.contains(e->pos())) continue;
        const Pomodoro& pm = m_pomos[pl.index];
        if (isGhost(pm)) break;   // un fantasma es parte del día: menú del día
        emit pomodoroMenuRequested(pm.id, e->globalPos());
        return;
    }
    const QDate d = dayAt(e->pos());
    if (d.isValid()) emit dayMenuRequested(d, e->globalPos());
}

void GridWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        for (const Placed& pl : m_placed)
            if (pl.rect.contains(e->position()) && isGhost(m_pomos[pl.index])) {
                emit goalClicked(m_pomos[pl.index].day);
                return;
            }
    }
    if (m_zoom == Zoom::Day || e->button() != Qt::LeftButton) return;
    for (const auto& [r, d] : m_dayRects) {
        if (r.contains(e->position())) {
            emit dayActivated(d);
            return;
        }
    }
}
