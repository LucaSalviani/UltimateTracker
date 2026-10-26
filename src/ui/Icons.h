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

QIcon get(Id id, const QColor& color = QColor(0xd8, 0xd8, 0xd8));

} // namespace icons
