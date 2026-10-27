#pragma once

#include <QColor>
#include <QFont>
#include <QString>
#include <QStringList>
#include <QVector>

// Temas de color. Cada paleta define unos pocos colores base y el resto (la
// escala de grises de superficies, bordes y textos) se deriva mezclando fondo y
// tinta, así todas las paletas mantienen la misma "jerarquía" que el tema oscuro.
//
// Los colores de abajo son variables (no constantes): theme::apply() los
// reescribe al cambiar de tema y los widgets pintados a mano los leen al pintar.
namespace theme {

struct Palette {
    QString key;           // "dark", "light", …
    QString name;          // nombre para mostrar
    QColor  bg;            // fondo de ventana
    QColor  ink;           // texto principal
    QColor  accent;        // pomodoro
    QColor  breakAccent;   // recreo
    QColor  squareDone;    // cuadradito terminado
    QColor  squareDoneEdge;
    QColor  squareFill;    // relleno del que corre
    QColor  squareFinal;   // último del día
    bool    light = false;
};

const QVector<Palette>& palettes();
// M15: el tema es color + modo. color: gray, ultra, pink, red, blue, gold.
// modo: dark, light o system (sigue a Windows).
QStringList colors();
QString keyFor(const QString& color, const QString& mode);
bool systemPrefersLight();
void migrateSetting(QString* color, QString* mode, const QString& oldKey);
const Palette& current();
// Aplica el tema: recalcula los colores, la paleta de Qt y la hoja de estilos.
void apply(const QString& key);

QColor mix(const QColor& a, const QColor& b, qreal t);

// ---- Colores vigentes (los reescribe apply) --------------------------------
inline QColor bg          {0x1c, 0x1c, 0x1c};  // fondo de ventana
inline QColor surface     {0x24, 0x24, 0x24};  // banda de fecha, tarjetas
inline QColor surfaceHi   {0x2e, 0x2e, 0x2e};  // hoy
inline QColor hover       {0x3a, 0x3a, 0x3a};  // fondo con el mouse encima
inline QColor hoverEdge   {0x6a, 0x6a, 0x6a};  // borde con el mouse encima
inline QColor line        {0x36, 0x36, 0x36};  // bordes sutiles
inline QColor text        {0xe8, 0xe8, 0xe8};
inline QColor textDim     {0x9a, 0x9a, 0x9a};
inline QColor textFaint   {0x62, 0x62, 0x62};
inline QColor iconInk     {0xd8, 0xd8, 0xd8};

// Cuadraditos
inline QColor squareEmpty   {0x2c, 0x2c, 0x2c};  // fondo del que se está llenando
inline QColor squareFill    {0x9a, 0x9a, 0x9a};  // relleno del que corre
inline QColor squareDone    {0x05, 0x05, 0x05};  // terminado
inline QColor squareDoneEdge{0x4a, 0x4a, 0x4a};
inline QColor squareFinal   {0xd2, 0x3b, 0x34};  // último del día
inline QColor bridge        {0x6a, 0x6a, 0x6a};  // puente de los encadenados
inline QColor accent        {0xd2, 0x3b, 0x34};

// Reproductor: cada modo tiene su color
inline QColor pomoAccent    {0xe0, 0x48, 0x3f};  // pomodoro
inline QColor breakAccent   {0x3c, 0xb8, 0x9b};  // recreo

// Escala del mapa de calor (de "nada" a "mucho")
inline QColor heat[5] = {{0x45, 0x45, 0x45}, {0x6a, 0x6a, 0x6a}, {0x95, 0x95, 0x95},
                         {0xc4, 0xc4, 0xc4}, {0xf2, 0xf2, 0xf2}};
inline QColor timelineBreak {0x55, 0x55, 0x55};

QFont uiFont(qreal pointSize, int weight = QFont::Normal);

} // namespace theme
