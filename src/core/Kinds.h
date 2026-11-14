#pragma once

#include "core/FolderTree.h"

#include <QString>
#include <QVector>

// Mejoras 3.0: tipo de actividad. No todo es "estudio": también se trabaja, se
// lee, se practica. Cada actividad tiene un tipo (las subactividades lo
// heredan) y los textos usan el verbo que corresponde. Donde se mezclan tipos
// (el día entero, los totales) se usa una palabra neutra: "foco".
namespace kinds {

struct Kind {
    QString id;        // "estudio", "trabajo", "lectura", "practica", "" (neutro) o "otro:Nombre|participio"
    QString name;      // "Estudio"
    QString noun;      // "estudio"      → "3h de estudio"
    QString done;      // "estudiado"    → "2h estudiado"
    QString doneFem;   // "estudiadas"   → "3h20 estudiadas hoy"
    QString vos;       // "estudiás"     → "Si hoy no estudiás…"
    QString past;      // "estudiaste"   → "Lo que más estudiaste"
    QString inf;       // "estudiar"     → "días sin estudiar"
};

QVector<Kind> builtin();                 // Estudio, Trabajo, Lectura, Práctica
Kind neutral();                          // foco
Kind byId(const QString& id);            // incluye los propios ("otro:…"); desconocido → Estudio
QString customId(const QString& name, const QString& participle);

// El tipo de una actividad: el suyo o el del ancestro más cercano que tenga
// uno. Sin ninguno: Estudio (como fue siempre).
QString idOf(const FolderTree& tree, qint64 folderId);
Kind of(const FolderTree& tree, qint64 folderId);
// El tipo común de varias actividades; si hay más de uno, el neutro.
Kind common(const FolderTree& tree, const QSet<qint64>& folderIds);
// El de todas las actividades de primer nivel (para textos generales).
Kind general(const FolderTree& tree);

} // namespace kinds
