#pragma once

#include <QIcon>

// Íconos dibujados en código: nítidos a cualquier escala y sin archivos extra.
namespace icons {

enum class Id {
    Play, Pause, Stop, ChevronLeft, ChevronRight, ZoomIn, ZoomOut,
    Proportional, Stats, Menu, Pin, Dock, Close, Folder, Back, AppLogo,
};

QIcon get(Id id, const QColor& color = QColor(0xd8, 0xd8, 0xd8));

} // namespace icons
