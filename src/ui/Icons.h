#pragma once

#include <QIcon>

// Íconos vectoriales dibujados en código: se pintan al tamaño y densidad exactos
// de la pantalla, así se ven nítidos a cualquier escala.
namespace icons {

enum class Id {
    Play, Pause, Stop, ChevronLeft, ChevronRight, ZoomIn, ZoomOut,
    Proportional, Stats, Menu, Pin, Dock, Minimize, Close, Folder, Back, AppLogo,
    Skip, Mute,
};

// Color que sigue al tema (se resuelve cada vez que se dibuja).
enum class Ink { Icon, Text, Dim };

QIcon get(Id id, Ink ink = Ink::Icon);
QIcon get(Id id, const QColor& fixedColor);   // color fijo (ej. blanco sobre el botón rojo)

} // namespace icons
