#pragma once

#include "core/FolderTree.h"
#include "core/Types.h"

#include <QSet>
#include <QString>
#include <QVector>

// M39: historial en CSV para abrir en Excel / Sheets.
// Separador ";" y decimales con coma (Excel en español los abre bien con doble
// click) y UTF-8 con BOM para que los acentos se vean.
namespace csvexport {

// Una fila por pomodoro (y por recreo / recreo indebido si includeBreaks),
// ordenadas por hora de inicio. folders != nullptr filtra los pomodoros.
QString build(const QVector<Pomodoro>& pomos, const QVector<Interval>& intervals, const FolderTree& tree,
              const QSet<qint64>* folders = nullptr, bool includeBreaks = true);

QString escape(const QString& field);

} // namespace csvexport
