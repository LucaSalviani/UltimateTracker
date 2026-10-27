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

QStringList shapeNames()    { return {"Clásico", "Inclinado", "Tarjeta", "Biselado"}; }
QStringList markNames()     { return {"Sin marca", "Diagonal cortada", "Mira", "Sello"}; }
QStringList fillNames()     { return {"Barra", "Líquido", "Segmentos", "Reloj"}; }
QStringList completeNames() { return {"Rebote", "Sellado", "Destello", "Volteo"}; }

namespace {
qreal ease(qreal t, QEasingCurve::Type type) { return QEasingCurve(type).valueForProgress(qBound<qreal>(0, t, 1)); }

QColor onColor(const QColor& bg) { return bg.lightness() < 128 ? QColor(255, 255, 255) : QColor(0, 0, 0); }

// Color de la marca de "hecho": el acento (como la rayita roja del boceto);
// sobre el rojo del último del día, blanco.
QColor markColor(State s)
{
    if (s == State::Final) return QColor(255, 255, 255, 230);
    QColor c = theme::squareFinal;
    c.setAlphaF(0.95);
    return c;
}

void drawLabel(QPainter& p, const QRectF& r, const Tile& t, const QColor& color, bool faint)
{
    const double s = qMin(r.width(), r.height());
    if (s < 15) return;
    const bool card = style().shape == Shape::Card;
    // Número del día arriba a la izquierda (todas las formas).
    if (t.number > 0 && s >= 30) {
        QFont nf = theme::uiFont(8, QFont::DemiBold);
        nf.setPixelSize(qMax(8, int(s * 0.17)));
        p.setFont(nf);
        QColor nc = color;
        nc.setAlphaF(0.55);
        p.setPen(nc);
        const double inset = style().shape == Shape::Slant ? r.height() * 0.16 : 0;   // la punta inclinada
        p.drawText(r.adjusted(s * 0.09 + inset * 0.6, s * 0.05, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                   card ? QStringLiteral("Nº%1").arg(t.number) : QString::number(t.number));
    }
    if (card) {
        // Tarjeta: tiempo grande abajo, actividad chiquita arriba a la derecha.
        if (!t.caption.isEmpty() && s >= 34) {
            QFont cf = theme::uiFont(7, QFont::DemiBold);
            cf.setPixelSize(qMax(7, int(s * 0.13)));
            cf.setCapitalization(QFont::AllUppercase);
            p.setFont(cf);
            QColor cc = color;
            cc.setAlphaF(faint ? 0.5 : 0.75);
            p.setPen(cc);
            const QRectF cr = r.adjusted(s * 0.46, s * 0.06, -s * 0.08, 0);
            p.drawText(cr, Qt::AlignRight | Qt::AlignTop,
                       QFontMetrics(cf).elidedText(t.caption, Qt::ElideRight, int(cr.width())));
        }
        QFont f = theme::uiFont(9, QFont::Bold);
        f.setPixelSize(qMax(7, int(s * 0.30)));
        p.setFont(f);
        p.setPen(color);
        p.drawText(r.adjusted(0, 0, -s * 0.09, -s * 0.10), Qt::AlignRight | Qt::AlignBottom, t.label);
        return;
    }
    QFont f = theme::uiFont(9, faint ? QFont::Medium : QFont::DemiBold);
    double px = s * (faint ? 0.26 : 0.30);
    f.setPixelSize(qMax(7, int(px)));
    QFontMetricsF fm(f);
    while (fm.horizontalAdvance(t.label) > r.width() * 0.80 && px > 7) {
        px -= 0.5;
        f.setPixelSize(int(px));
        fm = QFontMetricsF(f);
    }
    p.setFont(f);
    p.setPen(color);
    p.drawText(r, Qt::AlignCenter, t.label);
}

// Marca de "hecho". `amount` 0..1 la dibuja de a poco (animación de completado).
void drawMark(QPainter& p, const QRectF& r, State st, double amount)
{
    const Mark m = style().mark;
    if (m == Mark::None || amount <= 0) return;
    const double s = qMin(r.width(), r.height());
    if (s < 14) return;
    const QPointF c = r.center();
    const QColor col = markColor(st);
    const double pw = qMax(1.4, s * 0.045);
    p.save();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(col, pw, Qt::SolidLine, Qt::RoundCap));
    const bool card = style().shape == Shape::Card;
    switch (m) {
    case Mark::Diagonal: {
        // De esquina a esquina, cortada en el medio para no tapar el tiempo.
        const QPointF a(r.left() + r.width() * 0.16, r.bottom() - r.height() * (card ? 0.30 : 0.16));
        const QPointF b(r.right() - r.width() * 0.16, r.top() + r.height() * (card ? 0.30 : 0.16));
        const double gap = card ? 0.0 : 0.26;   // en la tarjeta el centro está libre
        auto at = [&](double u) { return a + (b - a) * u; };
        const double g1 = 0.5 - gap;
        const double f1 = qMin(1.0, amount * 2), f2 = qBound(0.0, amount * 2 - 1, 1.0);
        p.drawLine(at(0), at(g1 * f1));
        if (f2 > 0) p.drawLine(at(0.5 + gap), at(0.5 + gap + g1 * f2));
        break;
    }
    case Mark::Reticle: {
        // Mira: cuatro arcos y cuatro marcas hacia el centro, con el centro libre.
        const double R = s * 0.42;
        const QRectF ring(c.x() - R, c.y() - R, 2 * R, 2 * R);
        QPen thin(col, qMax(1.0, pw * 0.7), Qt::SolidLine, Qt::FlatCap);
        p.setPen(thin);
        for (int i = 0; i < 4; ++i) {
            const int start = (i * 90 + 14) * 16;
            p.drawArc(ring, start, int(62 * 16 * amount));
        }
        p.setPen(QPen(col, pw, Qt::SolidLine, Qt::RoundCap));
        // verticales hacia adentro; horizontales cortitas y por fuera del tiempo
        const double v0 = R * 0.72, v1 = R * (0.72 + 0.42 * amount);
        const double h0 = R * 0.94, h1 = R * (0.94 + 0.22 * amount);
        p.drawLine(QPointF(c.x(), c.y() - v1), QPointF(c.x(), c.y() - v0));
        p.drawLine(QPointF(c.x(), c.y() + v0), QPointF(c.x(), c.y() + v1));
        p.drawLine(QPointF(c.x() - h1, c.y()), QPointF(c.x() - h0, c.y()));
        p.drawLine(QPointF(c.x() + h0, c.y()), QPointF(c.x() + h1, c.y()));
        break;
    }
    case Mark::Stamp: {
        // Sello: un círculo con tilde, torcido, en la esquina de arriba a la derecha.
        // En la tarjeta va grande en el medio (como un "DONE!" estampado).
        const double d = card ? s * 0.56 : s * 0.34;
        const QPointF sc = card ? QPointF(c.x(), r.top() + r.height() * 0.46)
                                : QPointF(r.right() - d * 0.62, r.top() + d * 0.62);
        p.translate(sc);
        p.rotate(-14);
        p.scale(0.6 + 0.4 * amount, 0.6 + 0.4 * amount);
        QColor fillc = col;
        fillc.setAlphaF(0.16 * amount);
        p.setBrush(fillc);
        p.setPen(QPen(col, qMax(1.2, pw * 0.8)));
        p.drawEllipse(QPointF(0, 0), d / 2, d / 2);
        p.setPen(QPen(col, qMax(1.4, pw), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({QPointF(-d * 0.20, 0), QPointF(-d * 0.05, d * 0.15), QPointF(d * 0.22, -d * 0.14)}));
        break;
    }
    case Mark::None:
        break;
    }
    p.restore();
}

void fillRunning(QPainter& p, const QRectF& r, const QPainterPath& path, double progress, bool parked, qint64 now)
{
    const QColor fillc = parked ? theme::squareFill.darker(160) : theme::squareFill;
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
        // Sube desde abajo con una ola suave (dos capas desfasadas).
        const double level = r.bottom() - r.height() * pr;
        const double amp = parked ? 0 : r.height() * 0.035;
        for (int layer = 0; layer < 2; ++layer) {
            QPainterPath wave;
            wave.moveTo(r.left(), r.bottom());
            const double phase = now / (layer ? 520.0 : 760.0) + layer * 1.7;
            for (int i = 0; i <= 24; ++i) {
                const double x = r.left() + r.width() * i / 24.0;
                wave.lineTo(x, level + amp * std::sin(phase + i / 24.0 * 2 * M_PI) * (layer ? 0.8 : 1.0));
            }
            wave.lineTo(r.right(), r.bottom());
            wave.closeSubpath();
            QColor lc = fillc;
            if (layer == 0) lc.setAlphaF(0.45);
            p.setBrush(lc);
            p.drawPath(wave);
        }
        break;
    }
    case Fill::Segments: {
        // Como una batería: 5 bloques; el que se está llenando late.
        const int n = 5;
        const double gap = qMax(1.5, r.width() * 0.04);
        const double sw = (r.width() - gap * (n + 1)) / n;
        const double filled = pr * n;
        for (int i = 0; i < n; ++i) {
            const QRectF seg(r.left() + gap + i * (sw + gap), r.top() + gap, sw, r.height() - 2 * gap);
            QColor sc = fillc;
            if (i >= int(filled)) {
                if (i == int(filled) && !parked) {
                    const double frac = filled - int(filled);
                    sc.setAlphaF(0.25 + 0.55 * frac + 0.15 * std::sin(now / 260.0));
                } else {
                    sc = theme::mix(theme::squareEmpty, fillc, 0.12);
                }
            }
            p.setBrush(sc);
            p.drawRoundedRect(seg, 1.5, 1.5);
        }
        break;
    }
    case Fill::Clock: {
        // Barrido de reloj desde las 12.
        const double R = std::hypot(r.width(), r.height());
        const QRectF big(r.center().x() - R, r.center().y() - R, 2 * R, 2 * R);
        p.drawPie(big, 90 * 16, int(-360 * 16 * pr));
        // aguja fina
        if (!parked) {
            const double a = qDegreesToRadians(-90 + 360 * pr);
            QColor hand = theme::text;
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
    return style().shape == Shape::Card ? 0.78 : 1.0;
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
        // Romboide apoyado sobre su base, apenas inclinado hacia la derecha.
        const double k = r.height() * 0.16;
        QPolygonF poly({QPointF(r.left() + k, r.top()), QPointF(r.right(), r.top()),
                        QPointF(r.right() - k, r.bottom()), QPointF(r.left(), r.bottom())});
        path.addPolygon(poly);
        path.closeSubpath();
        break;
    }
    case Shape::Bevel: {
        // Esquinas de arriba-izquierda y abajo-derecha cortadas: aire de HUD.
        const double c = s * 0.20;
        QPolygonF poly({QPointF(r.left() + c, r.top()), QPointF(r.right(), r.top()),
                        QPointF(r.right(), r.bottom() - c), QPointF(r.right() - c, r.bottom()),
                        QPointF(r.left(), r.bottom()), QPointF(r.left(), r.top() + c)});
        path.addPolygon(poly);
        path.closeSubpath();
        break;
    }
    }
    return path;
}

bool animatesWhileRunning()
{
    return style().fill == Fill::Liquid || style().fill == Fill::Segments;
}

void paint(QPainter& p, const QRectF& rIn, const Tile& t, qint64 now)
{
    const QRectF r = rIn.adjusted(0.5, 0.5, -0.5, -0.5);
    const double s = qMin(r.width(), r.height());
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
            // Cae como un sello: grande → tamaño normal con rebote.
            const double k = 1.35 - 0.35 * ease(u * 1.8, QEasingCurve::OutBack);
            p.translate(c); p.scale(k, k); p.translate(-c);
            markAmount = ease((u - 0.35) / 0.5, QEasingCurve::OutCubic);
            break;
        }
        case Complete::Shine:
            markAmount = ease((u - 0.2) / 0.6, QEasingCurve::OutCubic);
            break;
        case Complete::Flip: {
            // Se da vuelta: primera mitad la cara "llena", segunda la de terminado.
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
    switch (st) {
    case State::Ghost: {
        QColor edge = theme::textFaint;
        edge.setAlphaF(0.7);
        QPen pen(edge, 1, Qt::DashLine);
        pen.setDashPattern({3, 3});
        p.setPen(pen);
        QColor fill = theme::text;
        fill.setAlphaF(0.025);
        p.setBrush(fill);
        p.drawPath(path);
        drawLabel(p, r, t, theme::textFaint, true);
        p.restore();
        return;
    }
    case State::Done:
    case State::Final: {
        const bool fin = st == State::Final;
        if (s >= 12) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 70));
            p.drawPath(path.translated(0, qMax(1.0, s * 0.035)));
        }
        QLinearGradient body(r.topLeft(), r.bottomLeft());
        if (fin) {
            body.setColorAt(0, theme::squareFinal.lighter(122));
            body.setColorAt(1, theme::squareFinal.darker(108));
        } else {
            body.setColorAt(0, theme::mix(theme::squareDone, theme::squareDoneEdge, 0.30));
            body.setColorAt(0.45, theme::squareDone);
            body.setColorAt(1, theme::squareDone);
        }
        p.setBrush(body);
        p.setPen(fin ? QPen(Qt::NoPen) : QPen(theme::squareDoneEdge, 1));
        p.drawPath(path);
        textColor = fin ? QColor(255, 255, 255)
                        : theme::mix(theme::squareDone, onColor(theme::squareDone), 0.6);
        break;
    }
    case State::Running:
    case State::Parked: {
        const bool parked = st == State::Parked;
        p.setBrush(theme::squareEmpty);
        p.setPen(QPen(theme::squareFill.darker(150), 1, parked ? Qt::DashLine : Qt::SolidLine));
        p.drawPath(path);
        // Durante el volteo, la cara de atrás se ve llena.
        fillRunning(p, r, path, t.completeT >= 0 ? 1.0 : t.progress, parked, now);
        if (parked && s >= 22) {
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

    // Rayita del color de la actividad abajo + filo de luz arriba.
    if (s >= 12) {
        p.save();
        p.setClipPath(path);
        p.setPen(Qt::NoPen);
        if (t.stripe.isValid()) {
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

    if (st == State::Done || st == State::Final) drawMark(p, r, st, markAmount);
    drawLabel(p, r, t, textColor, false);

    // Destello: un reflejo que cruza y un anillo que se abre.
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
        QColor ring = theme::squareFinal;
        ring.setAlphaF(0.6 * (1 - u));
        p.setPen(QPen(ring, 1.5));
        p.setBrush(Qt::NoBrush);
        const double grow = s * 0.25 * ease(u, QEasingCurve::OutCubic);
        p.drawPath(shapePath(r.adjusted(-grow, -grow, grow, grow)));
    }
    // Sellado: un fogonazo blanco que se apaga.
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
            } else {
                t.completeT = qMin(1.0, (cyc - 0.7) * 4000.0 / tiles::kCompleteMs);
            }
        }
        if (i == 3) t.state = tiles::State::Final;
        const QRectF r(4 + i * (w + gap), 4, w, h);
        tiles::paint(p, r, t, now);
    }
}
