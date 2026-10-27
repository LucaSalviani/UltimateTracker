#include "ui/Celebration.h"
#include "ui/Theme.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {
qreal clamp01(qreal v) { return qBound<qreal>(0.0, v, 1.0); }
// Progreso local de una etapa [a, b] (en ms) de la animación.
qreal stage(qreal ms, qreal a, qreal b) { return clamp01((ms - a) / (b - a)); }
qreal ease(qreal t, QEasingCurve::Type type) { return QEasingCurve(type).valueForProgress(t); }
} // namespace

namespace { Celebration::Style g_style = Celebration::Style::Visor; }
void Celebration::setStyle(Style s) { g_style = s; }
Celebration::Style Celebration::style() { return g_style; }
QStringList Celebration::styleNames()
{
    return {QStringLiteral("Visor (anillo y emblema)"), QStringLiteral("Línea"), QStringLiteral("Borde"),
            QStringLiteral("Tilde"), QStringLiteral("Cifra")};
}

Celebration::Celebration(QWidget* parent)
    : QWidget(parent)
{
    hide();
    m_frame.setInterval(16);
    connect(&m_frame, &QTimer::timeout, this, [this] {
        if (elapsed() >= m_duration) finish();
        else update();
    });
}

void Celebration::play(Kind kind, const QString& kicker, const QString& title, const QString& subtitle)
{
    m_kind = kind;
    m_kicker = kicker;
    m_title = title;
    m_subtitle = subtitle;
    m_accent = kind == Kind::Milestone ? QColor(0xe2, 0xb0, 0x4a)                 // dorado
             : kind == Kind::Goal      ? theme::breakAccent
                                       : theme::pomoAccent;
    m_duration = g_style == Style::Visor ? (kind == Kind::Pomodoro ? 2300 : 3000) : (kind == Kind::Pomodoro ? 1700 : 2300);
    // Los minimalistas no bloquean: se puede seguir usando la app mientras pasan.
    setAttribute(Qt::WA_TransparentForMouseEvents, g_style != Style::Visor);
    setGeometry(parentWidget()->rect());
    raise();
    show();
    m_clock.start();
    m_frame.start();
}

void Celebration::finish()
{
    m_frame.stop();
    hide();
}

void Celebration::mousePressEvent(QMouseEvent*)
{
    finish();
}

void Celebration::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal ms = elapsed();
    switch (g_style) {
    case Style::Visor:  paintVisor(p, ms); break;
    case Style::Line:   paintLine(p, ms); break;
    case Style::Border: paintBorder(p, ms); break;
    case Style::Tick:   paintTick(p, ms); break;
    case Style::Figure: paintFigure(p, ms); break;
    }
}

namespace {
// Texto chico: "POMODORO 08 COMPLETO · 25m"
void smallCaption(QPainter& p, const QRectF& r, const QString& kicker, const QString& title, const QColor& accent, qreal alpha)
{
    p.setOpacity(alpha);
    QFont kf = theme::uiFont(8, QFont::DemiBold);
    kf.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    QFont tf = theme::uiFont(9.5, QFont::DemiBold);
    const QString k = kicker.toUpper();
    const qreal kw = QFontMetricsF(kf).horizontalAdvance(k), tw = QFontMetricsF(tf).horizontalAdvance(title);
    const qreal total = kw + 14 + tw;
    qreal x = r.center().x() - total / 2;
    // Pastilla con fondo: se lee aunque detrás haya texto o cuadraditos.
    const QRectF pill(x - 12, r.top(), total + 24, r.height());
    p.setPen(QPen(theme::mix(theme::bg, theme::text, 0.16), 1));
    p.setBrush(theme::mix(theme::bg, theme::text, 0.05));
    p.drawRoundedRect(pill, r.height() / 2, r.height() / 2);
    p.setFont(kf);
    p.setPen(accent);
    p.drawText(QRectF(x, r.top(), kw + 2, r.height()), Qt::AlignLeft | Qt::AlignVCenter, k);
    x += kw + 14;
    p.setFont(tf);
    p.setPen(theme::text);
    p.drawText(QRectF(x, r.top(), tw + 4, r.height()), Qt::AlignLeft | Qt::AlignVCenter, title);
    p.setOpacity(1);
}
} // namespace

// Línea: un trazo fino del acento que se abre desde el centro, arriba, y un texto chico.
void Celebration::paintLine(QPainter& p, qreal ms)
{
    const qreal grow = ease(stage(ms, 0, 520), QEasingCurve::OutCubic);
    const qreal fade = 1.0 - stage(ms, m_duration - 450, m_duration);
    const qreal y = 1.5;   // el borde de arriba de la ventana
    const qreal half = width() * 0.5 * grow;
    QColor c = m_accent;
    c.setAlphaF(fade);
    p.setPen(QPen(c, 2.0, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(QPointF(width() / 2.0 - half, y), QPointF(width() / 2.0 + half, y));
    smallCaption(p, QRectF(0, 44, width(), 26), m_kicker, m_title, m_accent,
                 ease(stage(ms, 200, 500), QEasingCurve::OutCubic) * fade);
}

// Borde: el contorno de la ventana se dibuja una vez con el acento y se apaga.
void Celebration::paintBorder(QPainter& p, qreal ms)
{
    const qreal draw = ease(stage(ms, 0, 900), QEasingCurve::InOutCubic);
    const qreal fade = 1.0 - stage(ms, m_duration - 500, m_duration);
    const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
    QPainterPath path;
    path.addRoundedRect(r, 8, 8);
    QPainterPath part;
    const int steps = 120;
    const int upto = int(steps * draw);
    for (int i = 0; i <= upto; ++i) {
        const QPointF pt = path.pointAtPercent(qreal(i) / steps);
        if (i == 0) part.moveTo(pt); else part.lineTo(pt);
    }
    QColor c = m_accent;
    c.setAlphaF(0.9 * fade);
    p.setPen(QPen(c, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(part);
    smallCaption(p, QRectF(0, height() - 40, width(), 26), m_kicker, m_title, m_accent,
                 ease(stage(ms, 300, 650), QEasingCurve::OutCubic) * fade);
}

// Tilde: un círculo chico con tilde al centro, apenas oscurecido alrededor.
void Celebration::paintTick(QPainter& p, qreal ms)
{
    const qreal fade = 1.0 - stage(ms, m_duration - 400, m_duration);
    const qreal in = ease(stage(ms, 0, 260), QEasingCurve::OutCubic);
    p.fillRect(rect(), QColor(0, 0, 0, int(55 * in * fade)));
    const QPointF c(width() / 2.0, height() / 2.0 - 10);
    const qreal R = 22 * (0.85 + 0.15 * ease(stage(ms, 0, 420), QEasingCurve::OutBack));
    QColor ring = m_accent;
    ring.setAlphaF(fade);
    p.setPen(QPen(ring, 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(c.x() - R, c.y() - R, 2 * R, 2 * R), 90 * 16, int(-360 * 16 * ease(stage(ms, 0, 500), QEasingCurve::OutCubic)));
    const qreal draw = ease(stage(ms, 350, 700), QEasingCurve::OutCubic);
    QPainterPath check;
    check.moveTo(c + QPointF(-R * 0.40, R * 0.02));
    check.lineTo(c + QPointF(-R * 0.10, R * 0.32));
    check.lineTo(c + QPointF(R * 0.44, -R * 0.28));
    QPainterPath part;
    for (int i = 0; i <= int(24 * draw); ++i) {
        const QPointF pt = check.pointAtPercent(i / 24.0);
        if (i == 0) part.moveTo(pt); else part.lineTo(pt);
    }
    QColor ink = theme::text;
    ink.setAlphaF(fade);
    p.setPen(QPen(ink, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(part);
    smallCaption(p, QRectF(0, c.y() + R + 12, width(), 26), m_kicker, m_title, m_accent,
                 ease(stage(ms, 300, 600), QEasingCurve::OutCubic) * fade);
}

// Cifra: el tiempo ganado en grande y finito ("+25m"), que sube un poco y se va.
void Celebration::paintFigure(QPainter& p, qreal ms)
{
    const qreal in = ease(stage(ms, 0, 420), QEasingCurve::OutCubic);
    const qreal fade = 1.0 - stage(ms, m_duration - 500, m_duration);
    const qreal rise = -14 * ease(stage(ms, 0, m_duration), QEasingCurve::OutCubic);
    const QPointF c(width() / 2.0, height() / 2.0 - 12 + rise);
    QFont f;
    f.setFamilies({QStringLiteral("Bahnschrift"), QStringLiteral("Inter"), QStringLiteral("Segoe UI")});
    f.setPixelSize(int(qMin(height() * 0.22, 96.0)));
    f.setWeight(QFont::Light);
    f.setStretch(QFont::SemiCondensed);
    const QString big = m_kind == Kind::Pomodoro ? QStringLiteral("+") + m_title : m_title;
    p.fillRect(rect(), QColor(0, 0, 0, int(70 * in * fade)));   // apenas, para que la cifra se lea
    p.setOpacity(in * fade);
    p.setFont(f);
    QColor ink = theme::text;
    p.setPen(ink);
    const QRectF br(0, c.y() - f.pixelSize() * 0.7, width(), f.pixelSize() * 1.2);
    p.drawText(br, Qt::AlignCenter, big);
    QFont kf = theme::uiFont(8, QFont::DemiBold);
    kf.setLetterSpacing(QFont::AbsoluteSpacing, 2.4);
    p.setFont(kf);
    p.setPen(m_accent);
    p.drawText(QRectF(0, br.top() - 18, width(), 16), Qt::AlignHCenter | Qt::AlignBottom, m_kicker.toUpper());
    p.setOpacity(1);
}

void Celebration::paintVisor(QPainter& p, qreal ms)
{
    const qreal fadeOut = 1.0 - stage(ms, m_duration - 380, m_duration);
    const qreal fadeIn = ease(stage(ms, 0, 220), QEasingCurve::OutCubic);
    const qreal alpha = fadeIn * fadeOut;

    // Fondo: oscurece todo con una viñeta (el centro un poco más claro).
    const QPointF c(width() / 2.0, height() / 2.0 - height() * 0.06);
    QRadialGradient vignette(c, qMax(width(), height()) * 0.75);
    vignette.setColorAt(0, QColor(0, 0, 0, int(150 * alpha)));
    vignette.setColorAt(1, QColor(0, 0, 0, int(225 * alpha)));
    p.fillRect(rect(), vignette);

    const qreal R = qMin(width(), height()) * 0.20;   // radio del anillo

    // 1) Destello que se expande (onda)
    {
        const qreal t = stage(ms, 120, 900);
        if (t > 0 && t < 1) {
            QColor w = m_accent;
            w.setAlphaF(0.45 * (1 - t) * fadeOut);
            p.setPen(QPen(w, 2.0 + 6 * (1 - t)));
            p.setBrush(Qt::NoBrush);
            const qreal r = R * (0.8 + 1.1 * ease(t, QEasingCurve::OutCubic));
            p.drawEllipse(c, r, r);
        }
    }

    // 2) Anillo de visor: marcas que aparecen girando + arco que se dibuja
    {
        const qreal t = ease(stage(ms, 80, 760), QEasingCurve::OutCubic);
        const int ticks = 60;
        const int shown = int(ticks * t);
        const qreal spin = (1 - t) * 40.0;   // gira un poco mientras entra
        for (int i = 0; i < shown; ++i) {
            const qreal a = qDegreesToRadians(i * 360.0 / ticks - 90 + spin);
            const bool major = i % 5 == 0;
            const qreal r0 = R * (major ? 1.10 : 1.13), r1 = R * 1.18;
            QColor tc = major ? QColor(0xf0, 0xf0, 0xf0) : QColor(0xa0, 0xa0, 0xa0);
            tc.setAlphaF((major ? 0.85 : 0.45) * alpha);
            p.setPen(QPen(tc, major ? 1.6 : 1.0, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(c + QPointF(qCos(a) * r0, qSin(a) * r0), c + QPointF(qCos(a) * r1, qSin(a) * r1));
        }
        const qreal arc = ease(stage(ms, 200, 1050), QEasingCurve::InOutCubic);
        QColor ac = m_accent;
        ac.setAlphaF(alpha);
        p.setPen(QPen(ac, 3.0, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(c.x() - R, c.y() - R, 2 * R, 2 * R), 90 * 16, int(-360 * 16 * arc));
        // un puntito brillante en la punta del arco
        if (arc > 0 && arc < 1) {
            const qreal a = qDegreesToRadians(-90 + 360 * arc);
            QRadialGradient g(c + QPointF(qCos(a) * R, qSin(a) * R), 9);
            QColor gc = m_accent.lighter(140);
            gc.setAlphaF(0.9 * alpha);
            g.setColorAt(0, gc);
            g.setColorAt(1, Qt::transparent);
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawEllipse(c + QPointF(qCos(a) * R, qSin(a) * R), 9, 9);
        }
    }

    // 3) Emblema central: el cuadradito entra con rebote
    {
        const qreal t = stage(ms, 260, 760);
        const qreal s = ease(t, QEasingCurve::OutBack) * R * 0.95;
        if (s > 1) {
            p.save();
            p.translate(c);
            p.rotate((1 - ease(t, QEasingCurve::OutCubic)) * -18);
            const QRectF sq(-s / 2, -s / 2, s, s);
            const qreal rad = s * 0.16;
            QLinearGradient body(sq.topLeft(), sq.bottomLeft());
            body.setColorAt(0, m_accent.lighter(118));
            body.setColorAt(1, m_accent.darker(125));
            p.setPen(Qt::NoPen);
            QColor shadow(0, 0, 0, int(110 * alpha));
            p.setBrush(shadow);
            p.drawRoundedRect(sq.translated(0, s * 0.06), rad, rad);
            p.setOpacity(alpha);
            p.setBrush(body);
            p.drawRoundedRect(sq, rad, rad);
            // brillo diagonal que pasa una vez (como el reflejo de un lente)
            const qreal sweep = stage(ms, 700, 1250);
            if (sweep > 0 && sweep < 1) {
                QPainterPath clip;
                clip.addRoundedRect(sq, rad, rad);
                p.setClipPath(clip);
                const qreal x = -s + 2.2 * s * ease(sweep, QEasingCurve::InOutSine);
                QLinearGradient shine(QPointF(x - s * 0.3, 0), QPointF(x + s * 0.3, 0));
                shine.setColorAt(0, Qt::transparent);
                shine.setColorAt(0.5, QColor(255, 255, 255, 120));
                shine.setColorAt(1, Qt::transparent);
                p.setBrush(shine);
                p.drawRect(sq);
                p.setClipping(false);
            }
            // borde de luz arriba
            QColor hl(255, 255, 255, 70);
            p.setPen(QPen(hl, 1.2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(sq.adjusted(1, 1, -1, -1), rad - 1, rad - 1);
            // símbolo: ✓ para pomodoro/meta, número para hito
            p.setPen(QPen(QColor(255, 255, 255), qMax(2.0, s * 0.07), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            if (m_kind == Kind::Milestone) {
                QFont f = theme::uiFont(10, QFont::Bold);
                f.setPixelSize(int(s * 0.34));
                p.setFont(f);
                p.drawText(sq, Qt::AlignCenter, m_title.section(' ', 0, 0));
            } else {
                const qreal draw = ease(stage(ms, 560, 900), QEasingCurve::OutCubic);
                QPainterPath check;
                check.moveTo(-s * 0.20, s * 0.02);
                check.lineTo(-s * 0.05, s * 0.17);
                check.lineTo(s * 0.22, -s * 0.14);
                QPainterPath part;
                const int steps = 24;
                for (int i = 0; i <= int(steps * draw); ++i) {
                    const QPointF pt = check.pointAtPercent(qreal(i) / steps);
                    if (i == 0) part.moveTo(pt); else part.lineTo(pt);
                }
                p.drawPath(part);
            }
            p.restore();
        }
    }

    // 4) Textos: entran desde abajo, con letras espaciadas
    {
        const qreal t = ease(stage(ms, 520, 980), QEasingCurve::OutCubic);
        if (t > 0) {
            p.setOpacity(t * fadeOut);
            const qreal y0 = c.y() + R * 1.42 + (1 - t) * 14;
            QFont kf = theme::uiFont(8.5, QFont::DemiBold);
            kf.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
            p.setFont(kf);
            p.setPen(m_accent.lighter(130));
            p.drawText(QRectF(0, y0, width(), 20), Qt::AlignHCenter | Qt::AlignTop, m_kicker.toUpper());

            QFont tf = theme::uiFont(qBound(14.0, R * 0.16, 26.0), QFont::Bold);
            p.setFont(tf);
            p.setPen(QColor(0xf4, 0xf4, 0xf4));
            const qreal th = QFontMetricsF(tf).height();
            p.drawText(QRectF(0, y0 + 20, width(), th + 4), Qt::AlignHCenter | Qt::AlignTop, m_title);

            QFont sf = theme::uiFont(9.5);
            p.setFont(sf);
            p.setPen(QColor(0xb4, 0xb4, 0xb4));
            p.drawText(QRectF(0, y0 + 24 + th, width(), 20), Qt::AlignHCenter | Qt::AlignTop, m_subtitle);
            p.setOpacity(1);
        }
    }
}
