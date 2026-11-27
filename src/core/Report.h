#pragma once

#include "core/Database.h"

#include <QString>

// Informe completo: todo el historial en un solo .html con gráficos (SVG en línea),
// tablas, diario, notas, ánimo, libreta y rutina; más CSVs para Excel.
namespace report {

// Página HTML autocontenida (se abre en el navegador; imprimir → "Guardar como PDF").
QString html(const Database& db, const QDateTime& now);

// CSV de una fila por día: estudio, recreos, indebido, inactivo, pomodoros, sesiones, ánimo y nota del diario.
QString daysCsv(const Database& db, const QDateTime& now);
// CSV por actividad: horas directas, horas con subactividades, pomodoros.
QString activitiesCsv(const Database& db);

// Escribe la carpeta del informe (informe.html, pomodoros.csv, dias.csv, actividades.csv, libreta.txt).
// Devuelve "" si salió bien o el motivo del error.
QString writeAll(const Database& db, const QString& dir, const QDateTime& now);

} // namespace report
