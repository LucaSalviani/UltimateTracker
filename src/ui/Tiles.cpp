#include "ui/Tiles.h"
#include "ui/Theme.h"

#include <QEasingCurve>
#include <QPainter>
#include <QtMath>

namespace tiles {

Style& style()
{
    static Style s;
    return s;
}

// Cuánto se corre arriba el inclinado (lo usan la forma y los segmentos).
double slantOffset(const QRectF& r) { return r.height() * 0.10; }

QStringList shapeNames()    { return {"Clásico", "Inclinado", "Tarjeta", "Biselado"}; }
QStringList markNames()     { return {"Sin marca", "Diagonal", "Cruz", "Tilde redondo", "Tilde cuadrado"}; }
QStringList fillNames()     { return {"Barra", "Líquido", "Segmentos", "Reloj"}; }
QStringList completeNames() { return {"Rebote", "Sellado", "Destello", "Volteo"}; }
QStringList paletteNames()  { return {"Del tema", "Naranja", "Rojo", "Azul", "Verde", "Papel (blanco)"}; }
QStringList borderNames()   { return {"Fino", "Normal", "Grueso"}; }

QString numberText(int n)
{
    return style().numberPrefix ? QStringLiteral("Nº%1").arg(n) : QString::number(n);
}

QFont font(qreal px, int weight)
{
    QFont f;
    f.setFamilies({QStringLiteral("Bahnschrift"), QStringLiteral("DIN Alternate"), QStringLiteral("Inter"),
                   QStringLiteral("Segoe UI"), QStringLiteral("sans-serif")});
    f.setPixelSize(qMax(6, int(px)));
    f.setWeight(QFont::Weight(weight));
    f.setStretch(QFont::SemiCondensed);
    return f;
}

namespace {
qreal ease(qreal t, QEasingCurve::Type type) { return QEasingCurve(type).valueForProgress(qBound<qreal>(0, t, 1)); }
QColor onColor(const QColor& bg)
{
    // Luminancia percibida: el rojo del pomodoro lleva texto blanco, el papel negro.
    const double lum = 0.299 * bg.red() + 0.587 * bg.green() + 0.114 * bg.blue();
    return lum < 165 ? QColor(255, 255, 255) : QColor(0x14, 0x14, 0x14);
}

// ---- Colores de los cuadraditos: los del tema o una paleta propia (M23)
struct Colors { QColor done, doneEdge, final, fill, empty; };
Colors colors()
{
    Colors c{theme::squareDone, theme::squareDoneEdge, theme::squareFinal, theme::squareFill, theme::squareEmpty};
    switch (style().palette) {
    case 1: c.done = {0xff, 0x8a, 0x00}; c.doneEdge = {0xc8, 0x6a, 0x00}; break;                       // naranja
    case 2: c.done = {0xd8, 0x2a, 0x20}; c.doneEdge = {0x9a, 0x18, 0x12}; c.final = {0x14, 0x14, 0x14}; break;  // rojo
    case 3: c.done = {0x2f, 0x6f, 0xe8}; c.doneEdge = {0x1d, 0x4c, 0xa8}; break;                       // azul
    case 4: c.done = {0x22, 0xa0, 0x6b}; c.doneEdge = {0x16, 0x72, 0x4c}; break;                       // verde
    case 5: c.done = {0xf4, 0xf4, 0xf2}; c.doneEdge = {0xc8, 0xc8, 0xc4}; break;                       // papel
    default: break;
    }
    if (style().palette != 0) c.fill = theme::mix(c.done, QColor(0x80, 0x80, 0x80), 0.35);
    return c;
}
// Resaltado del que corre: el color del pomodoro; con la paleta roja, negro (M31).
QColor accentFill() { return style().palette == 2 ? QColor(0x14, 0x14, 0x14) : theme::pomoAccent; }

double borderWidth() { return style().border == 0 ? 0.7 : style().border == 2 ? 2.2 : 1.0; }

// Polígono con las esquinas redondeadas (para inclinado y biselado "amigables").
QPainterPath roundedPolygon(const QVector<QPointF>& pts, double radius)
{
    QPainterPath path;
    const int n = int(pts.size());
    for (int i = 0; i < n; ++i) {
        const QPointF prev = pts[(i + n - 1) % n], cur = pts[i], next = pts[(i + 1) % n];
        const QPointF v1 = prev - cur, v2 = next - cur;
        const double l1 = std::hypot(v1.x(), v1.y()), l2 = std::hypot(v2.x(), v2.y());
        const double r = qMin(radius, qMin(l1, l2) / 2);
        const QPointF a = cur + v1 / l1 * r, b = cur + v2 / l2 * r;
        if (i == 0) path.moveTo(a); else path.lineTo(a);
        path.quadTo(cur, b);
    }
    path.closeSubpath();
    return path;
}

// ---- Textos: número arriba al centro, tiempo al medio, actividad abajo
void drawLabel(QPainter& p, const QRectF& r, const Tile& t, const QColor& color, bool faint, const QColor& halo = {})
{
    const double s = qMin(r.width(), r.height());
    if (s < 15) return;
    const bool card = style().shape == Shape::Card;
    const bool caption = !t.caption.isEmpty() && style().caption && s >= 34;   // M29: solo si está elegido
    if (t.number > 0 && s >= 30 && style().numberPos == 0) {
        QFont nf = font(s * 0.17, QFont::DemiBold);
        p.setFont(nf);
        QColor nc = color;
        nc.setAlphaF(0.55);
        p.setPen(nc);
        p.drawText(r.adjusted(0, s * 0.05, 0, 0), Qt::AlignHCenter | Qt::AlignTop, numberText(t.number));
    }
    if (caption) {
        QFont cf = font(s * 0.13, QFont::DemiBold);
        cf.setCapitalization(QFont::AllUppercase);
        p.setFont(cf);
        QColor cc = color;
        cc.setAlphaF(faint ? 0.45 : 0.65);
        p.setPen(cc);
        const QRectF cr = card ? r.adjusted(s * 0.08, s * 0.25, -s * 0.08, 0)
                               : r.adjusted(s * 0.08, 0, -s * 0.08, -s * 0.11);
        p.drawText(cr, Qt::AlignHCenter | (card ? Qt::AlignTop : Qt::AlignBottom),
                   QFontMetrics(cf).elidedText(t.caption, Qt::ElideRight, int(cr.width())));
    }
    double px = s * (faint ? 0.28 : 0.33);
    QFont f = font(px, faint ? QFont::Medium : (card ? QFont::Bold : QFont::DemiBold));
    QFontMetricsF fm(f);
    while (fm.horizontalAdvance(t.label) > r.width() * 0.80 && px > 7) {
        px -= 0.5;
        f.setPixelSize(int(px));
        fm = QFontMetricsF(f);
    }
    p.setFont(f);
    const QRectF lr = card ? r.adjusted(0, 0, 0, -s * 0.12)
                           : r.adjusted(0, caption ? -s * 0.04 : 0, 0, caption ? -s * 0.04 : 0);
    const Qt::Alignment al = card ? (Qt::AlignHCenter | Qt::AlignBottom) : Qt::AlignCenter;
    if (halo.isValid()) {
        // Halo: el texto se lee aunque quede mitad sobre lleno y mitad sobre vacío.
        const QRectF br = fm.boundingRect(lr, al, t.label);
        QPainterPath tp;
        tp.addText(br.left(), br.top() + fm.ascent(), f, t.label);
        p.save();
        p.setPen(QPen(halo, qMax(2.0, f.pixelSize() * 0.16), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(color);
        p.drawPath(tp);
        p.setPen(Qt::NoPen);
        p.drawPath(tp);
        p.restore();
        return;
    }
    p.setPen(color);
    p.drawText(lr, al, t.label);
}

// ---- Marca de "hecho". `amount` 0..1 la dibuja de a poco (completado).
void drawMark(QPainter& p, const QRectF& r, State st, double amount, const QColor& numberColor)
{
    const Mark m = style().mark;
    if (m == Mark::None || amount <= 0) return;
    const double s = qMin(r.width(), r.height());
    if (s < 14) return;
    const QPointF c = r.center();
    const bool card = style().shape == Shape::Card;
    // Delicadas: del color del número (no colorinche). Los tildes sí van con acento.
    QColor soft = numberColor;
    soft.setAlphaF(0.60);
    const double thin = qMax(1.0, s * 0.022);
    p.save();
    p.setBrush(Qt::NoBrush);
    // Línea de a a b cortada en el medio (para no tapar el tiempo).
    auto cutLine = [&](QPointF a, QPointF b, double gap, double amt) {
        auto at = [&](double u) { return a + (b - a) * u; };
        const double g1 = 0.5 - gap;
        const double f1 = qMin(1.0, amt * 2), f2 = qBound(0.0, amt * 2 - 1, 1.0);
        p.drawLine(at(0), at(g1 * f1));
        if (f2 > 0) p.drawLine(at(0.5 + gap), at(0.5 + gap + g1 * f2));
    };
    const double inset = 0.20;
    const QPointF tl(r.left() + r.width() * inset, r.top() + r.height() * inset);
    const QPointF br(r.right() - r.width() * inset, r.bottom() - r.height() * inset);
    const QPointF tr(r.right() - r.width() * inset, r.top() + r.height() * inset);
    const QPointF bl(r.left() + r.width() * inset, r.bottom() - r.height() * inset);
    const double gap = card ? 0.14 : 0.24;
    switch (m) {
    case Mark::Diagonal:   // de arriba-izquierda a abajo-derecha, finita
        p.setPen(QPen(soft, thin, Qt::SolidLine, Qt::RoundCap));
        cutLine(tl, br, gap, amount);
        break;
    case Mark::Cross:      // cruz delicada (las dos diagonales, centro libre)
        p.setPen(QPen(soft, thin, Qt::SolidLine, Qt::RoundCap));
        cutLine(tl, br, gap, amount);
        cutLine(tr, bl, gap, amount);
        break;
    case Mark::Stamp:
    case Mark::SquareTick: {
        // Tilde en un círculo o en un cuadradito, torcido, arriba a la derecha
        // (en la tarjeta, grande al medio).
        QColor col = st == State::Final ? QColor(255, 255, 255, 230) : colors().final;
        if (style().palette == 2 && st != State::Final) col = QColor(255, 255, 255, 230);
        const double d = card ? s * 0.50 : s * 0.30;
        const QPointF sc = card ? QPointF(c.x(), r.top() + r.height() * 0.50)
                                : QPointF(r.right() - d * 0.66, r.top() + d * 0.66);
        p.translate(sc);
        if (m == Mark::Stamp) p.rotate(-14);   // el cuadrado va derechito (M29)
        p.scale(0.6 + 0.4 * amount, 0.6 + 0.4 * amount);
        QColor fillc = col;
        fillc.setAlphaF(0.16 * amount);
        p.setBrush(fillc);
        if (m == Mark::Stamp) {
            p.setPen(QPen(col, qMax(1.1, s * 0.03)));
            p.drawEllipse(QPointF(0, 0), d / 2, d / 2);
            p.setPen(QPen(col, qMax(1.3, s * 0.04), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(QPolygonF({QPointF(-d * 0.20, 0), QPointF(-d * 0.05, d * 0.15), QPointF(d * 0.22, -d * 0.14)}));
        } else {
            // Cuadrado recto con un tilde de ángulo recto (piernas perpendiculares, puntas rectas).
            p.setPen(QPen(col, qMax(1.1, s * 0.03), Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
            p.drawRect(QRectF(-d / 2, -d / 2, d, d));
            p.setPen(QPen(col, qMax(1.3, s * 0.045), Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
            p.drawPolyline(QPolygonF({QPointF(-d * 0.24, -d * 0.02), QPointF(-d * 0.08, d * 0.14), QPointF(d * 0.24, -d * 0.18)}));
        }
        break;
    }
    case Mark::None:
        break;
    }
    p.restore();
}

// ---- Llenado del que corre
void fillRunning(QPainter& p, const QRectF& r, const QPainterPath& path, double progress, bool parked, qint64 now,
                 bool t_accent)
{
    const Colors cs = colors();
    // Invertido (por defecto): fondo clarito que se llena oscuro, como si se cargara.
    QColor fillc = style().inverted ? cs.done : cs.fill;
    if (t_accent) fillc = accentFill();   // M29: el que está corriendo, con el color del pomodoro
    if (parked) fillc = theme::mix(fillc, style().inverted ? cs.fill : cs.empty, 0.45);
    p.save();
    p.setClipPath(path);
    p.setPen(Qt::NoPen);
    p.setBrush(fillc);
    const double pr = qBound(0.0, progress, 1.0);
    switch (style().fill) {
    case Fill::Bar:
        p.drawRect(QRectF(r.left(), r.top(), r.width() * pr, r.height()));
        break;
    case Fill::Liquid: {
        // Avanza de izquierda a derecha con el frente ondulado (dos capas que se mueven).
        const double front = r.left() + r.width() * pr;
        const double amp = parked ? 0 : qMax(1.5, r.width() * 0.05);
        for (int layer = 0; layer < 2; ++layer) {
            QPainterPath wave;
            wave.moveTo(r.left(), r.top());
            const double phase = now / (layer ? 330.0 : 470.0) + layer * 2.1;
            const int steps = 28;
            for (int i = 0; i <= steps; ++i) {
                const double y = r.top() + r.height() * i / double(steps);
                const double x = front + amp * std::sin(phase + i / double(steps) * 2.4 * M_PI) * (layer ? 0.7 : 1.0)
                                       - (layer ? amp * 0.6 : 0);
                wave.lineTo(qMax(r.left(), x), y);
            }
            wave.lineTo(r.left(), r.bottom());
            wave.closeSubpath();
            QColor lc = fillc;
            if (layer == 0) lc.setAlphaF(0.45);
            p.setBrush(lc);
            p.drawPath(wave);
        }
        break;
    }
    case Fill::Segments: {
        // M32 "contorno": los vacíos son solo un borde fino, los llenos sólidos y el
        // que se está llenando se carga de abajo hacia arriba. Se lee de un vistazo
        // cuánto falta, sin transparencias. En el inclinado, inclinados.
        const int n = 5;
        const QColor segFill = t_accent ? accentFill() : theme::mix(cs.fill, QColor(255, 255, 255), 0.15);
        const QColor edge = theme::mix(cs.empty, segFill, parked ? 0.30 : 0.50);
        const double gap = qMax(1.5, r.width() * 0.035);
        const double skew = style().shape == Shape::Slant ? slantOffset(r) : 0.0;
        const double usable = r.width() - skew;
        const double swk = (usable - gap * (n + 1)) / n;
        const double filled = pr * n;
        auto segPath = [&](const QRectF& seg) {
            QPainterPath sp;
            if (skew > 0) {
                const double kk = skew * (seg.height() / r.height());
                sp.addPolygon(QPolygonF({QPointF(seg.left() + kk, seg.top()), QPointF(seg.right() + kk, seg.top()),
                                         QPointF(seg.right(), seg.bottom()), QPointF(seg.left(), seg.bottom())}));
                sp.closeSubpath();
            } else {
                sp.addRoundedRect(seg, 1.5, 1.5);
            }
            return sp;
        };
        for (int i = 0; i < n; ++i) {
            const QRectF seg(r.left() + gap + i * (swk + gap), r.top() + gap, swk, r.height() - 2 * gap);
            const QPainterPath sp = segPath(seg.adjusted(0.5, 0.5, -0.5, -0.5));
            QColor solid = parked ? segFill.darker(150) : segFill;
            if (i < int(filled)) {
                p.setPen(Qt::NoPen);
                p.setBrush(solid);
                p.drawPath(sp);
                continue;
            }
            p.setPen(QPen(edge, 1));
            p.setBrush(Qt::NoBrush);
            p.drawPath(sp);
            if (i == int(filled)) {
                const double frac = filled - int(filled);
                p.save();
                p.setClipPath(sp);
                p.setPen(Qt::NoPen);
                p.setBrush(solid);
                p.drawRect(QRectF(seg.left() - skew, seg.bottom() - seg.height() * frac, seg.width() + 2 * skew, seg.height() * frac));
                p.restore();
            }
        }
        break;
    }
    case Fill::Clock: {
        const double R = std::hypot(r.width(), r.height());
        const QRectF big(r.center().x() - R, r.center().y() - R, 2 * R, 2 * R);
        p.drawPie(big, 90 * 16, int(-360 * 16 * pr));
        if (!parked) {
            const double a = qDegreesToRadians(-90 + 360 * pr);
            QColor hand = onColor(style().inverted ? cs.fill : cs.empty);
            hand.setAlphaF(0.55);
            p.setPen(QPen(hand, 1.2));
            p.drawLine(r.center(), r.center() + QPointF(std::cos(a), std::sin(a)) * R);
        }
        break;
    }
    }
    p.restore();
}
} // namespace

double aspect()
{
    if (style().shape == Shape::Card) return 0.78;
    if (style().shape == Shape::Slant) return 1.10;   // apenas más ancho
    return 1.0;
}

QPainterPath shapePath(const QRectF& r)
{
    QPainterPath path;
    const double s = qMin(r.width(), r.height());
    switch (style().shape) {
    case Shape::Classic:
        path.addRoundedRect(r, qMax(1.0, s * 0.12), qMax(1.0, s * 0.12));
        break;
    case Shape::Card:
        path.addRoundedRect(r, qMax(1.0, s * 0.08), qMax(1.0, s * 0.08));
        break;
    case Shape::Slant: {
        // Romboide apoyado sobre su base, apenas inclinado y con esquinas suaves.
        const double k = slantOffset(r);
        path = roundedPolygon({QPointF(r.left() + k, r.top()), QPointF(r.right(), r.top()),
                               QPointF(r.right() - k, r.bottom()), QPointF(r.left(), r.bottom())},
                              s * 0.11);
        break;
    }
    case Shape::Bevel: {
        // Dos esquinas cortadas (arriba-izquierda y abajo-derecha), suavizadas.
        const double c = s * 0.15;
        path = roundedPolygon({QPointF(r.left() + c, r.top()), QPointF(r.right(), r.top()),
                               QPointF(r.right(), r.bottom() - c), QPointF(r.right() - c, r.bottom()),
                               QPointF(r.left(), r.bottom()), QPointF(r.left(), r.top() + c)},
                              s * 0.07);
        break;
    }
    }
    return path;
}

bool animatesWhileRunning()
{
    return style().fill == Fill::Liquid;   // los segmentos ya no laten
}

void paint(QPainter& p, const QRectF& rIn, const Tile& t, qint64 now)
{
    const QRectF r = rIn.adjusted(0.5, 0.5, -0.5, -0.5);
    const double s = qMin(r.width(), r.height());
    const Colors cs = colors();
    const double bw = borderWidth();
    State st = t.state;

    // ---- Animación de completado: transforma el cuadradito entero
    double markAmount = 1.0;
    p.save();
    if (t.completeT >= 0 && t.completeT < 1 && (st == State::Done || st == State::Final)) {
        const double u = t.completeT;
        const QPointF c = r.center();
        switch (style().complete) {
        case Complete::Pop: {
            const double k = 1.0 + 0.18 * std::sin(M_PI * qMin(1.0, u * 1.6)) * (1 - u);
            p.translate(c); p.scale(k, k); p.translate(-c);
            markAmount = ease(u * 1.5, QEasingCurve::OutCubic);
            break;
        }
        case Complete::Seal: {
            const double k = 1.35 - 0.35 * ease(u * 1.8, QEasingCurve::OutBack);
            p.translate(c); p.scale(k, k); p.translate(-c);
            markAmount = ease((u - 0.35) / 0.5, QEasingCurve::OutCubic);
            break;
        }
        case Complete::Shine:
            markAmount = ease((u - 0.2) / 0.6, QEasingCurve::OutCubic);
            break;
        case Complete::Flip: {
            const double sx = std::abs(std::cos(M_PI * u));
            p.translate(c); p.scale(qMax(0.02, sx), 1.0); p.translate(-c);
            if (u < 0.5) st = State::Running;
            markAmount = ease((u - 0.5) / 0.4, QEasingCurve::OutCubic);
            break;
        }
        }
    }

    const QPainterPath path = shapePath(r);
    QColor textColor;
    QColor runningHalo;
    switch (st) {
    case State::Ghost: {
        QColor edge = theme::textFaint;
        edge.setAlphaF(0.7);
        QPen pen(edge, bw, Qt::DashLine);
        pen.setDashPattern({3, 3});
        p.setPen(pen);
        QColor fill = theme::text;
        fill.setAlphaF(0.025);
        p.setBrush(fill);
        p.drawPath(path);
        if (t.progress > 0) {   // bloque del objetivo empezado (por tiempo acumulado)
            p.save();
            p.setClipPath(path);
            QColor pf = theme::textFaint;
            pf.setAlphaF(0.30);
            p.setPen(Qt::NoPen);
            p.setBrush(pf);
            p.drawRect(QRectF(r.left(), r.top(), r.width() * qMin(1.0, t.progress), r.height()));
            p.restore();
        }
        drawLabel(p, r, t, theme::textFaint, true);
        p.restore();
        return;
    }
    case State::Done:
    case State::Final: {
        const bool fin = st == State::Final;
        const QColor base = fin ? cs.final : cs.done;
        if (s >= 12) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 70));
            p.drawPath(path.translated(0, qMax(1.0, s * 0.035)));
        }
        QLinearGradient body(r.topLeft(), r.bottomLeft());
        body.setColorAt(0, fin || style().palette != 0 ? base.lighter(116) : theme::mix(base, cs.doneEdge, 0.30));
        body.setColorAt(0.45, base);
        body.setColorAt(1, fin ? base.darker(108) : base);
        p.setBrush(body);
        p.setPen(fin ? QPen(base.darker(125), bw) : QPen(cs.doneEdge, bw));
        p.drawPath(path);
        textColor = fin ? onColor(base) : theme::mix(base, onColor(base), style().palette == 0 ? 0.6 : 0.92);
        break;
    }
    case State::Running:
    case State::Parked: {
        const bool parked = st == State::Parked;
        // Los segmentos van siempre sobre oscuro (como les queda bien); el resto, invertido si se eligió.
        const QColor bg = style().inverted && style().fill != Fill::Segments ? cs.fill : cs.empty;
        p.setBrush(bg);
        p.setPen(QPen(style().inverted ? bg.darker(118) : cs.fill.darker(150), bw, parked ? Qt::DashLine : Qt::SolidLine));
        p.drawPath(path);
        fillRunning(p, r, path, t.completeT >= 0 ? 1.0 : t.progress, parked, now, t.accent && !parked);
        if (parked && s >= 22) {
            const double g = s * 0.16, w = g * 0.32;
            const QPointF o(r.right() - s * 0.1 - g, r.top() + s * 0.1);
            p.setPen(Qt::NoPen);
            p.setBrush(onColor(bg));
            p.drawRect(QRectF(o.x(), o.y(), w, g));
            p.drawRect(QRectF(o.x() + g * 0.62, o.y(), w, g));
        }
        // El texto contrasta con lo que tenga detrás (mitad llena, mitad vacía).
        const QColor under = t.accent && !parked ? accentFill()
                           : (style().inverted && style().fill != Fill::Segments ? cs.done : cs.fill);
        textColor = parked ? theme::mix(onColor(under), under, 0.4) : onColor(under);
        runningHalo = under;
        if (t.accent && !parked) {
            // M31: sobre el resaltado, texto negro y sin halo (con la paleta roja, blanco sobre negro).
            textColor = style().palette == 2 ? QColor(0xf2, 0xf2, 0xf2) : QColor(0x14, 0x14, 0x14);
            runningHalo = QColor();
        }
        break;
    }
    }

    // Rayita del color de la actividad abajo (se puede apagar) + filo de luz arriba.
    if (s >= 12) {
        p.save();
        p.setClipPath(path);
        p.setPen(Qt::NoPen);
        if (style().stripe && t.stripe.isValid()) {
            QColor stripe = t.stripe;
            stripe.setAlphaF(st == State::Final ? 0.55 : 0.9);
            p.setBrush(stripe);
            const double sh = qMax(2.0, s * 0.075);
            p.drawRect(QRectF(r.left() - 2, r.bottom() - sh, r.width() + 4, sh + 1));
        }
        p.setBrush(QColor(255, 255, 255, st == State::Final ? 60 : 22));
        p.drawRect(QRectF(r.left(), r.top(), r.width(), 1.0));
        p.restore();
    }

    if (st == State::Done || st == State::Final) drawMark(p, r, st, markAmount, textColor);
    drawLabel(p, r, t, textColor, false, runningHalo);

    if (t.completeT >= 0 && t.completeT < 1 && style().complete == Complete::Shine) {
        const double u = t.completeT;
        p.save();
        p.setClipPath(path);
        const double x = r.left() - r.width() + 3.0 * r.width() * ease(u / 0.7, QEasingCurve::InOutSine);
        QLinearGradient shine(QPointF(x - s * 0.35, r.top()), QPointF(x + s * 0.35, r.bottom()));
        shine.setColorAt(0, Qt::transparent);
        shine.setColorAt(0.5, QColor(255, 255, 255, 150));
        shine.setColorAt(1, Qt::transparent);
        p.setPen(Qt::NoPen);
        p.setBrush(shine);
        p.drawRect(r);
        p.restore();
        QColor ring = cs.final;
        ring.setAlphaF(0.6 * (1 - u));
        p.setPen(QPen(ring, 1.5));
        p.setBrush(Qt::NoBrush);
        const double grow = s * 0.25 * ease(u, QEasingCurve::OutCubic);
        p.drawPath(shapePath(r.adjusted(-grow, -grow, grow, grow)));
    }
    if (t.completeT >= 0 && t.completeT < 0.5 && style().complete == Complete::Seal) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, int(150 * (1 - t.completeT * 2))));
        p.drawPath(path);
    }
    p.restore();
}

} // namespace tiles

// ===========================================================================
TilePreview::TilePreview(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(86);
    m_clock.start();
    m_timer.setInterval(33);
    connect(&m_timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    m_timer.start();
}

void TilePreview::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qint64 now = m_clock.elapsed();
    const double h = height() - 14;
    const double w = h * tiles::aspect();
    const double gap = 10;
    // Un ciclo de 4 s: el tercero se llena y se completa, una y otra vez.
    const double cyc = (now % 4000) / 4000.0;
    const QColor stripes[] = {{0x6f, 0x9b, 0xd1}, {0x8f, 0xc2, 0x8a}, {0xd1, 0x8f, 0x6f}, {0xc7, 0x9b, 0xd6}};
    for (int i = 0; i < 4; ++i) {
        tiles::Tile t;
        t.number = i + 1;
        t.label = i == 1 ? QStringLiteral("50m") : QStringLiteral("25m");
        t.caption = i == 1 ? QStringLiteral("Inglés") : QStringLiteral("Facultad");
        t.stripe = stripes[i];
        t.state = tiles::State::Done;
        if (i == 2) {
            if (cyc < 0.7) {
                t.state = tiles::State::Running;
                t.progress = cyc / 0.7;
                t.accent = tiles::style().highlight == 1;
            } else {
                t.completeT = qMin(1.0, (cyc - 0.7) * 4000.0 / tiles::kCompleteMs);
            }
        }
        if (i == 3 && tiles::style().highlight == 2) t.state = tiles::State::Final;
        if (tiles::style().numberPos != 0) t.number = 0;
        const QRectF r(4 + i * (w + gap), 4, w, h);
        tiles::paint(p, r, t, now);
    }
}
