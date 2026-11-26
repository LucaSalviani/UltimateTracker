#pragma once

#include "core/Database.h"
#include "core/Routine.h"

#include <QColor>
#include <QDate>
#include <QWidget>

// Mejoras 6.0: ventanita para crear o editar un evento de la rutina, al estilo de Google Calendar
// (título grande, categoría con color, horario, repetición, «termina el…», borrar).
namespace routinedlg {

QColor colorFor(const QString& category);   // cada categoría, siempre el mismo color suave
// true si se guardó o borró algo. id = 0 crea uno nuevo, con `day` como día sugerido.
bool edit(Database* db, qint64 id, const QDate& day, QWidget* parent);

} // namespace routinedlg
