#pragma once

#include "core/Types.h"

#include <QHash>
#include <QSet>
#include <QVector>

// Vista en memoria del árbol de carpetas. Se reconstruye cuando cambia la base.
class FolderTree {
public:
    FolderTree() = default;
    explicit FolderTree(const QVector<Folder>& folders);

    const QVector<Folder>& all() const { return m_folders; }
    bool contains(qint64 id) const { return m_byId.contains(id); }
    const Folder* find(qint64 id) const;

    QVector<qint64> children(qint64 parentId) const;   // ordenados
    QSet<qint64>    subtree(qint64 id) const;           // id + descendientes
    QVector<qint64> ancestorsAndSelf(qint64 id) const;  // [id, padre, abuelo, ...]
    QString         path(qint64 id, const QString& sep = QStringLiteral(" › ")) const;
    int             depth(qint64 id) const;

    // Suma las horas directas de cada carpeta hacia arriba:
    // total(Facultad) = directo(Facultad) + total de cada materia, etc.
    QHash<qint64, qint64> rollUp(const QHash<qint64, qint64>& direct) const;

private:
    QVector<Folder>                 m_folders;
    QHash<qint64, int>              m_byId;
    QHash<qint64, QVector<qint64>>  m_children;
};
