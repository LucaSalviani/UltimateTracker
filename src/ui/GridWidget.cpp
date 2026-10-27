#include "ui/GridWidget.h"
#include "core/DayClock.h"
#include "core/Format.h"
#include "ui/Theme.h"
#include "ui/Tiles.h"

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
void GridWidget::setSizeMode(SizeMode m) { m_sizeMode = m; update(); }

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

void GridWidget::setGoalBlocks(const QHash<QDate, QVector<int>>& blocks, const QHash<QDate, double>& firstPartial)
{
    m_goalBlocks = blocks;
    m_goalPartial = firstPartial;
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
            g.plannedSec = it.value()[k] * 60;
            // M21: el primero que falta puede estar a medias (por tiempo acumulado).
            g.studiedSec = k == 0 ? int(g.plannedSec * m_goalPartial.value(it.key(), 0.0)) : 0;
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
    if (tiles::style().highlight != 2) return false;   // M29: el rojo del último del día es opcional
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
                                            double headerH, bool breakLines, double maxSide, double minSide,
                                            bool groupGaps) const
{
    Layout out;
    int count = 0;
    for (const auto& g : groups) count += int(g.size());
    if (count == 0 || area.width() <= 4 || area.height() <= 4) return out;

    // M16: tres modos de tamaño. Uniforme; solo ancho (largo ∝ minutos, alto fijo);
    // proporcional completo (lado ∝ √minutos → área ∝ minutos). 25' = tamaño base.
    QHash<int, double> w, h;
    double maxW = 1.0, maxH = 1.0;
    for (const auto& g : groups)
        for (int idx : g) {
            const Pomodoro& pmx = m_pomos[idx];
            const double ratio = qMax(60, isGhost(pmx) ? pmx.plannedSec : pmx.representedSec()) / 1500.0;
            double vw = 1.0, vh = 1.0 / tiles::aspect();   // la tarjeta es vertical
            if (m_sizeMode == SizeMode::Area) { vw = std::sqrt(ratio); vh = vw / tiles::aspect(); }
            else if (m_sizeMode == SizeMode::Width) vw = qBound(0.45, ratio, 4.0);
            w.insert(idx, vw);
            h.insert(idx, vh);
            maxW = qMax(maxW, vw);
            maxH = qMax(maxH, vh);
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
        if (lay) lay->gap = gap;
        for (const auto& g : groups) {
            if (g.isEmpty()) continue;
            // Recuadros en línea: un poco de aire entre actividades para que no se toquen los bordes.
            if (!breakLines && groupGaps && !firstGroup && x > area.left()) x += gap * 1.2;
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
                const double sh = k * h.value(idx);
                // M29: número afuera, arriba del cuadradito: se le hace lugar.
                const double numH = (tiles::style().numberPos == 1 && !isGhost(m_pomos[idx])) ? qBound(9.0, k * 0.26, 15.0) : 0;
                // Un pomodoro encadenado (sin recreo) va pegado al anterior.
                if (i > 0 && m_pomos[idx].linked && x > area.left()) x -= gap - kLinkGap;
                if (x + s > area.right() + 0.01 && x > area.left()) {
                    closeRow();
                    x = area.left();
                    y += rowH + gap;
                    rowH = 0;
                }
                if (lay) lay->placed.append({QRectF(x, y, qMin(s, area.width()), sh), idx});
                x += s + gap;
                rowH = qMax(rowH, sh + numH);
            }
        }
        closeRow();
        return y + rowH <= area.bottom() + 0.01;
    };

    maxSide *= m_baseScale;   // M23: tamaño base elegido en Ajustes
    double hi = maxSide / (m_sizeMode == SizeMode::Width ? maxH : maxW);
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

// El dibujo de cada cuadradito vive en Tiles (M18): forma, marca, llenado y
// animación de completado se eligen en Ajustes.
void GridWidget::paintSquare(QPainter& p, const QRectF& r, const Pomodoro& pomo) const
{
    tiles::Tile t;
    if (isGhost(pomo)) {
        t.state = tiles::State::Ghost;
        t.label = fmt::duration(pomo.plannedSec);
        t.progress = pomo.plannedSec > 0 ? double(pomo.studiedSec) / pomo.plannedSec : 0;
        tiles::paint(p, r, t, m_clock.elapsed());
        return;
    }
    if (isDayEnd(pomo)) t.state = tiles::State::Final;
    else if (pomo.status == PomoStatus::Running) t.state = tiles::State::Running;
    else if (pomo.status == PomoStatus::Parked) t.state = tiles::State::Parked;
    else t.state = tiles::State::Done;
    t.progress = pomo.id == m_liveId ? m_liveProgress
                                     : (pomo.plannedSec > 0 ? double(pomo.studiedSec) / pomo.plannedSec : 0.0);
    t.label = fmt::duration(pomo.representedSec());
    t.caption = m_names.value(pomo.folderId);
    t.number = tiles::style().numberPos == 0 ? m_number.value(pomo.id) : 0;   // afuera lo pinta la grilla
    t.stripe = m_colors.value(pomo.folderId);
    t.accent = tiles::style().highlight == 1 && pomo.status == PomoStatus::Running;
    if (m_completeAt.contains(pomo.id)) {
        const qint64 dt = m_clock.elapsed() - m_completeAt.value(pomo.id);
        t.completeT = dt < 0 ? -1 : qMin(1.0, double(dt) / tiles::kCompleteMs);
        if (dt < 0) t.state = tiles::State::Running, t.progress = 1.0;   // espera a que termine el festejo
    }
    tiles::paint(p, r, t, m_clock.elapsed());
}

void GridWidget::markCompleted(qint64 id, int delayMs)
{
    if (m_completeAt.contains(id)) return;
    m_completeAt.insert(id, m_clock.elapsed() + delayMs);
    update();
}

// ---------------------------------------------------------------------------
// M30: rompecabezas (skyline). Unidades: el lado de un pomodoro de 25' = 1.
// ---------------------------------------------------------------------------
namespace {
struct Item { double w, h; int ref; };
struct Placement { double x, y; };

// Coloca cada pieza en el punto más bajo del "horizonte" (y, a igualdad, más a la izquierda).
double skylinePack(const QVector<Item>& items, double binW, QVector<Placement>* out)
{
    struct Seg { double x, y, w; };
    QVector<Seg> sky{{0, 0, binW}};
    double height = 0;
    out->resize(items.size());
    for (int n = 0; n < items.size(); ++n) {
        const Item& it = items[n];
        double bestY = 1e18, bestX = 0;
        for (int i = 0; i < sky.size(); ++i) {
            const double x = sky[i].x;
            if (x + it.w > binW + 1e-9) break;
            double y = 0;
            for (int j = i; j < sky.size() && sky[j].x < x + it.w - 1e-9; ++j) y = qMax(y, sky[j].y);
            if (y < bestY - 1e-9 || (qAbs(y - bestY) < 1e-9 && x < bestX)) { bestY = y; bestX = x; }
        }
        if (bestY > 1e17) { bestY = height; bestX = 0; }   // más ancha que el contenedor: abajo de todo
        (*out)[n] = {bestX, bestY};
        height = qMax(height, bestY + it.h);
        // Actualizar el horizonte: el tramo [bestX, bestX + w) pasa a la altura nueva.
        QVector<Seg> next;
        const double x0 = bestX, x1 = bestX + it.w;
        for (const Seg& sg : sky) {
            const double a = sg.x, b = sg.x + sg.w;
            if (b <= x0 + 1e-9 || a >= x1 - 1e-9) { next.append(sg); continue; }
            if (a < x0) next.append({a, sg.y, x0 - a});
            if (b > x1) next.append({x1, sg.y, b - x1});
        }
        next.append({x0, bestY + it.h, x1 - x0});
        std::sort(next.begin(), next.end(), [](const Seg& l, const Seg& r) { return l.x < r.x; });
        sky = next;
    }
    return height;
}

// Prueba varios anchos y se queda con el que deja el bloque más cuadrado y compacto.
QSizeF packSquare(QVector<Item> items, QVector<Placement>* out, QVector<int>* order)
{
    // Las piezas grandes primero encastran mejor.
    QVector<int> idx(items.size());
    for (int i = 0; i < idx.size(); ++i) idx[i] = i;
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return items[a].w * items[a].h > items[b].w * items[b].h; });
    QVector<Item> sorted;
    for (int i : idx) sorted.append(items[i]);
    double area = 0, maxW = 0, sumW = 0;
    for (const Item& it : sorted) { area += it.w * it.h; maxW = qMax(maxW, it.w); sumW += it.w; }
    double bestScore = 1e18;
    QSizeF best;
    QVector<Placement> bestPos;
    for (int k = 0; k <= 16; ++k) {
        const double W = maxW + (sumW - maxW) * k / 16.0;
        QVector<Placement> pos;
        const double H = skylinePack(sorted, W, &pos);
        double usedW = 0;
        for (int i = 0; i < sorted.size(); ++i) usedW = qMax(usedW, pos[i].x + sorted[i].w);
        const double ratio = qMax(usedW, H) / qMax(1e-9, qMin(usedW, H));
        const double waste = (usedW * H) / qMax(1e-9, area);
        const double score = ratio * ratio * waste;
        if (score < bestScore) { bestScore = score; best = QSizeF(usedW, H); bestPos = pos; }
    }
    *out = bestPos;
    *order = idx;
    return best;
}
} // namespace

GridWidget::Layout GridWidget::layoutPuzzle(const QRectF& area, const QVector<QVector<int>>& groups,
                                            double maxSide, double minSide) const
{
    // Todas las piezas en un solo horizonte, en orden de actividad (y dentro de
    // cada una, las grandes primero): así cada actividad queda junta y el total
    // encastra como un rompecabezas lo más cuadrado posible.
    Layout out;
    const double gapU = 0.12;
    QVector<Item> items;
    for (const auto& g : groups) {
        QVector<Item> gi;
        for (int idx : g) {
            const Pomodoro& pm = m_pomos[idx];
            const double side = std::sqrt(qMax(60, isGhost(pm) ? pm.plannedSec : pm.representedSec()) / 1500.0);
            gi.append({side + gapU, side + gapU, idx});
        }
        std::stable_sort(gi.begin(), gi.end(), [](const Item& l, const Item& r) { return l.w > r.w; });
        items += gi;
    }
    if (items.isEmpty()) return out;
    double area2 = 0, maxW = 0, sumW = 0;
    for (const Item& it : items) { area2 += it.w * it.h; maxW = qMax(maxW, it.w); sumW += it.w; }
    double bestScore = 1e18;
    QVector<Placement> bestPos;
    QSizeF best;
    for (int k = 0; k <= 24; ++k) {
        const double W = maxW + (sumW - maxW) * k / 24.0;
        QVector<Placement> pos;
        const double H = skylinePack(items, W, &pos);
        double usedW = 0;
        for (int i = 0; i < items.size(); ++i) usedW = qMax(usedW, pos[i].x + items[i].w);
        const double ratio = qMax(usedW, H) / qMax(1e-9, qMin(usedW, H));
        const double waste = (usedW * H) / qMax(1e-9, area2);
        const double score = ratio * waste * waste;   // prioriza que no queden huecos, después lo cuadrado
        if (score < bestScore) { bestScore = score; bestPos = pos; best = QSizeF(usedW, H); }
    }
    const double k = qMax(minSide * 0.5, qMin(qMin(area.width() / qMax(0.1, best.width()),
                                                   area.height() / qMax(0.1, best.height())),
                                              maxSide * m_baseScale));
    out.gap = gapU * k;
    for (int i = 0; i < items.size(); ++i) {
        const double side = (items[i].w - gapU) * k;
        out.placed.append({QRectF(area.left() + bestPos[i].x * k, area.top() + bestPos[i].y * k, side, side), items[i].ref});
    }
    return out;
}

void GridWidget::paintSquares(QPainter& p, const QRectF& area, const QDate& day, double maxSide, double minSide,
                              double headerH, bool breakLines)
{
    // Grupos: por actividad en el orden en que aparecen; los fantasmas de la meta al final.
    const QVector<int> all = m_byDay.value(day);
    QVector<QVector<int>> groups;
    QVector<int> ghosts;
    // Proporcional completo: siempre en un bloque tipo tetris, con recuadros (sin títulos).
    GroupStyle gs = m_groupStyle;
    if (m_sizeMode == SizeMode::Area && gs != GroupStyle::None) gs = GroupStyle::Boxes;
    if (gs != GroupStyle::None) {
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
    const bool titled = gs == GroupStyle::Titles && groups.size() >= 2;
    const bool boxed = gs == GroupStyle::Boxes && !groups.isEmpty();
    const int realGroups = int(groups.size());
    if (!ghosts.isEmpty()) {
        if (titled || boxed) groups.append(ghosts);
        else if (groups.isEmpty()) groups.append(ghosts);
        else groups.last() += ghosts;
    }
    // Con recuadros se deja margen para el contorno (si no, queda cortado en el borde).
    const Layout lay = m_sizeMode == SizeMode::Area
        ? layoutPuzzle(area.adjusted(5, 5, -5, -5), groups, maxSide, minSide)
        : boxed ? layoutGroups(area.adjusted(5, 5, -5, -5), groups, 0, false, maxSide, minSide, true)
                             : layoutGroups(area, groups, titled ? headerH : 0, breakLines && titled, maxSide, minSide);

    p.save();
    p.setClipRect(area.adjusted(-2, -2, 2, 2));
    for (const Header& h : lay.headers) paintHeader(p, h);
    if (boxed) {
        // Contorno que abraza los cuadraditos de cada actividad (se unen como en el tetris).
        const double pad = lay.gap * 0.6;
        QHash<qint64, QPainterPath> outline;
        QHash<qint64, int> order;
        for (const Placed& pl : lay.placed) {
            const Pomodoro& pm = m_pomos[pl.index];
            if (isGhost(pm)) continue;
            // Rectángulos rectos que se solapan: la unión queda limpia, sin muescas.
            QPainterPath r;
            r.addRect(pl.rect.adjusted(-pad, -pad, pad, pad));
            outline[pm.folderId] = outline.value(pm.folderId).united(r);
            order.insert(pm.folderId, 0);
        }
        Q_UNUSED(realGroups);
        for (auto it = outline.cbegin(); it != outline.cend(); ++it) {
            QColor c = m_colors.value(it.key(), theme::textFaint);
            c.setAlphaF(0.85);
            p.setPen(QPen(c, 1.0, Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(it.value().simplified());
        }
    }
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
        // Repintar seguido mientras haya un completado en curso o un llenado que se mueve.
        if (m_completeAt.contains(pm.id) && m_clock.elapsed() - m_completeAt.value(pm.id) < tiles::kCompleteMs)
            m_animating = true;
        if ((pm.status == PomoStatus::Running || (pm.id == m_liveId)) && tiles::animatesWhileRunning())
            m_animating = true;
        if (scale <= 0.01) { m_placed.append(pl); continue; }
        // M29: número afuera, arriba (como en los bocetos: "Nº1").
        if (tiles::style().numberPos == 1 && !isGhost(pm) && m_number.value(pm.id) > 0 && pl.rect.width() >= 18) {
            const double nh = qBound(9.0, pl.rect.height() * 0.24, 15.0);
            QFont nf = tiles::font(nh * 0.85, QFont::Bold);   // M31: centrado y en negrita, sin itálica
            p.setFont(nf);
            p.setOpacity(opacity);
            p.setPen(theme::textDim);
            p.drawText(QRectF(pl.rect.left() - 10, pl.rect.top() - nh - 1, pl.rect.width() + 20, nh),
                       Qt::AlignHCenter | Qt::AlignBottom, tiles::numberText(m_number.value(pm.id)));
            p.setOpacity(1.0);
        }
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
        m_completeAt.clear();
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
                               (pm.studiedSec > 0 ? QStringLiteral("Objetivo: bloque de %1 · llevás %2\nClick para ver el objetivo del día")
                                                     .arg(fmt::duration(pm.plannedSec), fmt::duration(pm.studiedSec))
                                                  : QStringLiteral("Objetivo: falta un bloque de %1\nClick para ver el objetivo del día")
                                                     .arg(fmt::duration(pm.plannedSec))), this);
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
