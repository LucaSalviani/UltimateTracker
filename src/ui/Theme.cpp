#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QPainter>
#include <QImage>
#include <QStandardPaths>
#include <QDir>
#include <QHash>
#include <QPalette>
#include <QRegularExpression>
#include <QStringList>
#include <QWidget>

namespace theme {

namespace {
QString g_current = QStringLiteral("dark");

QString hex(const QColor& c) { return c.name(QColor::HexRgb); }
} // namespace

QColor mix(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

const QVector<Palette>& palettes()
{
    static const QVector<Palette> list = {
        // key, nombre, fondo, tinta, pomodoro, recreo, terminado, borde, relleno, último, claro
        {"dark",  QStringLiteral("Oscuro"),       {0x1c, 0x1c, 0x1c}, {0xe8, 0xe8, 0xe8}, {0xe0, 0x48, 0x3f},
         {0x3c, 0xb8, 0x9b}, {0x05, 0x05, 0x05}, {0x4a, 0x4a, 0x4a}, {0x9a, 0x9a, 0x9a}, {0xd2, 0x3b, 0x34}, false},
        {"light", QStringLiteral("Día"),          {0xf3, 0xf2, 0xef}, {0x1f, 0x1f, 0x1f}, {0xd9, 0x44, 0x3b},
         {0x23, 0x96, 0x7c}, {0x2a, 0x2a, 0x2a}, {0x2a, 0x2a, 0x2a}, {0x8c, 0x8c, 0x8c}, {0xd2, 0x3b, 0x34}, true},
        {"ultra", QStringLiteral("Ultra oscuro"), {0x05, 0x05, 0x05}, {0xd4, 0xd4, 0xd4}, {0xe0, 0x48, 0x3f},
         {0x3c, 0xb8, 0x9b}, {0x00, 0x00, 0x00}, {0x2c, 0x2c, 0x2c}, {0x78, 0x78, 0x78}, {0xc4, 0x34, 0x2d}, false},
        {"pink",  QStringLiteral("Rosa"),         {0x1f, 0x19, 0x1d}, {0xf4, 0xe6, 0xee}, {0xec, 0x62, 0x9e},
         {0x5c, 0xc2, 0xb0}, {0x10, 0x0b, 0x0e}, {0x55, 0x3d, 0x4b}, {0xb8, 0x96, 0xaa}, {0xec, 0x62, 0x9e}, false},
        {"red",   QStringLiteral("Rojo"),         {0x1e, 0x16, 0x16}, {0xf2, 0xe4, 0xe2}, {0xe5, 0x3a, 0x2f},
         {0x3c, 0xb8, 0x9b}, {0x0e, 0x07, 0x07}, {0x55, 0x36, 0x34}, {0xad, 0x90, 0x8d}, {0xe5, 0x3a, 0x2f}, false},
        {"blue",  QStringLiteral("Azul"),         {0x15, 0x19, 0x21}, {0xe3, 0xe9, 0xf3}, {0x4f, 0x8f, 0xff},
         {0x3c, 0xc2, 0xa0}, {0x07, 0x0a, 0x10}, {0x36, 0x43, 0x5c}, {0x8e, 0x9c, 0xb6}, {0x4f, 0x8f, 0xff}, false},
        {"gold",  QStringLiteral("Mostaza"),      {0x1d, 0x19, 0x13}, {0xf2, 0xe7, 0xcf}, {0xda, 0xa0, 0x30},
         {0x8f, 0xb8, 0x5a}, {0x0d, 0x0a, 0x06}, {0x52, 0x46, 0x30}, {0xb6, 0xa3, 0x78}, {0xda, 0xa0, 0x30}, false},
    };
    return list;
}

const Palette& current()
{
    for (const Palette& p : palettes())
        if (p.key == g_current) return p;
    return palettes().first();
}

void apply(const QString& key)
{
    g_current = key;
    const Palette& P = current();
    const QColor B = P.bg, I = P.ink;
    auto m = [&](qreal t) { return mix(B, I, t); };

    // ---- colores para lo que se pinta a mano
    bg          = B;
    surface     = m(0.035);
    surfaceHi   = m(0.075);
    hover       = m(0.13);
    hoverEdge   = m(0.30);
    line        = m(0.09);
    text        = I;
    textDim     = m(0.60);
    textFaint   = m(0.36);
    iconInk     = m(0.90);
    squareEmpty = m(0.07);
    squareFill  = P.squareFill;
    squareDone  = P.squareDone;
    squareDoneEdge = P.squareDoneEdge;
    squareFinal = P.squareFinal;
    bridge      = m(0.30);
    accent      = P.squareFinal;
    pomoAccent  = P.accent;
    breakAccent = P.breakAccent;
    const qreal heatT[5] = {0.16, 0.30, 0.48, 0.70, 0.95};
    for (int i = 0; i < 5; ++i) heat[i] = m(heatT[i]);
    timelineBreak = m(0.22);

    // ---- paleta de Qt (diálogos, menús nativos)
    QPalette pal;
    pal.setColor(QPalette::Window, B);
    pal.setColor(QPalette::WindowText, I);
    pal.setColor(QPalette::Base, m(0.025));
    pal.setColor(QPalette::AlternateBase, B);
    pal.setColor(QPalette::Text, I);
    pal.setColor(QPalette::Button, m(0.06));
    pal.setColor(QPalette::ButtonText, I);
    pal.setColor(QPalette::ToolTipBase, m(0.06));
    pal.setColor(QPalette::ToolTipText, I);
    pal.setColor(QPalette::Highlight, m(0.15));
    pal.setColor(QPalette::HighlightedText, I);
    pal.setColor(QPalette::PlaceholderText, m(0.40));
    pal.setColor(QPalette::Link, I);
    for (auto role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText})
        pal.setColor(QPalette::Disabled, role, m(0.30));
    QApplication::setPalette(pal);

    // ---- hoja de estilos: plantilla con @tokens
    QFile f(QStringLiteral(":/style.qss"));
    if (!f.open(QIODevice::ReadOnly)) return;
    QString qss = QString::fromUtf8(f.readAll());
    const QColor extreme = P.light ? QColor(0, 0, 0) : QColor(0xff, 0xff, 0xff);
    const QHash<QString, QColor> tokens = {
        {"bg", B},           {"bg2", m(0.02)},     {"s1", m(0.035)},    {"s2", m(0.045)},
        {"s3", m(0.06)},     {"s4", m(0.08)},      {"s5", m(0.10)},     {"s6", m(0.13)},
        {"s7", m(0.18)},     {"s8", m(0.28)},      {"t3", m(0.58)},     {"t2", m(0.82)},
        {"t1", I},           {"t0", mix(I, extreme, 0.6)},              {"bgx", mix(B, extreme == QColor(0, 0, 0) ? QColor(255, 255, 255) : QColor(0, 0, 0), 0.4)},
        {"acc", P.accent},   {"accHi", P.accent.lighter(112)},          {"accLo", P.accent.darker(115)},
        {"accText", mix(P.accent, I, 0.25)},
        {"brk", P.breakAccent}, {"brkHi", P.breakAccent.lighter(112)},  {"brkLo", P.breakAccent.darker(115)},
        {"danger", QColor(0xc4, 0x2b, 0x1c)}, {"dangerBg", mix(B, QColor(0xc4, 0x2b, 0x1c), 0.25)},
        {"dangerEdge", mix(B, QColor(0xc4, 0x2b, 0x1c), 0.5)},
    };
    // Tilde de las casillas: un PNG chiquito del color del fondo (contrasta con el relleno).
    // PNG y no SVG: no depende del plugin de SVG de Qt.
    {
        const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(QStringLiteral("ultimatetracker-check-%1.png").arg(P.key));
        QImage img(14, 14, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter g(&img);
        g.setRenderHint(QPainter::Antialiasing);
        g.setPen(QPen(B, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g.drawPolyline(QPolygonF({{2.8, 7.4}, {5.7, 10.2}, {11.4, 4.0}}));
        g.end();
        img.save(path, "PNG");
        qss.replace(QStringLiteral("@checkImg"), QDir::fromNativeSeparators(path));
    }
    // Reemplazo de los tokens más largos primero (@accHi antes que @acc).
    QStringList keys = tokens.keys();
    std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) { return a.size() > b.size(); });
    for (const QString& k : keys) qss.replace('@' + k, hex(tokens.value(k)));
    qApp->setStyleSheet(qss);
    // Lo pintado a mano lee los colores al pintar: basta con repintar.
    for (QWidget* w : QApplication::allWidgets()) w->update();
}

QFont uiFont(qreal pointSize, int weight)
{
    QFont f;
    f.setFamilies({QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"),
                   QStringLiteral("Inter"), QStringLiteral("Noto Sans"), QStringLiteral("sans-serif")});
    f.setPointSizeF(pointSize);
    f.setWeight(QFont::Weight(weight));
    return f;
}

} // namespace theme
