#pragma once

class QWidget;

// M36: transición suave. Antes de cambiar el contenido se saca una foto de cómo
// se ve, se la pone encima y se la desvanece: el contenido nuevo aparece
// debajo, ya pintado. No hay pantallazo vacío ni espera.
namespace fade {
void cross(QWidget* w, int ms = 180);
}
