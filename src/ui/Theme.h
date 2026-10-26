#pragma once

#include <QColor>
#include <QFont>

// Paleta gris. Los colores de la grilla viven acá para tocarlos en un solo lugar.
namespace theme {

inline const QColor bg          {0x1c, 0x1c, 0x1c};  // fondo de ventana
inline const QColor surface     {0x24, 0x24, 0x24};  // banda de fecha, tarjetas
inline const QColor surfaceHi   {0x2e, 0x2e, 0x2e};  // hover / hoy
inline const QColor line        {0x36, 0x36, 0x36};  // bordes sutiles
inline const QColor text        {0xe8, 0xe8, 0xe8};
inline const QColor textDim     {0x9a, 0x9a, 0x9a};
inline const QColor textFaint   {0x62, 0x62, 0x62};

// Cuadraditos
inline const QColor squareEmpty   {0x2c, 0x2c, 0x2c};  // fondo del que se está llenando
inline const QColor squareFill    {0x9a, 0x9a, 0x9a};  // relleno gris del que corre
inline const QColor squareDone    {0x05, 0x05, 0x05};  // negro: completo
inline const QColor squareDoneEdge{0x4a, 0x4a, 0x4a};
inline const QColor squareFinal   {0xd2, 0x3b, 0x34};  // rojo: final de sesión
inline const QColor accent        {0xd2, 0x3b, 0x34};

QFont uiFont(qreal pointSize, int weight = QFont::Normal);

} // namespace theme
