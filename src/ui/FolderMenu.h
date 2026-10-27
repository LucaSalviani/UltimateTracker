#pragma once

#include "core/FolderTree.h"

#include <QMenu>

// Menú desplegable con el árbol de actividades. Las que tienen subactividades
// se abren como submenú, cuyo primer ítem es la actividad misma.
class FolderMenu : public QMenu {
    Q_OBJECT
public:
    explicit FolderMenu(QWidget* parent = nullptr);
    // badges: texto extra al lado de algunas actividades (ej. "⏸ 12m").
    void rebuild(const FolderTree& tree, qint64 current, const QHash<qint64, QString>& badges = {});

signals:
    void folderChosen(qint64 id);
    void manageRequested();

private:
    void addLevel(QMenu* menu, const FolderTree& tree, qint64 parent, qint64 current);
    QString label(const Folder* f) const;
    QHash<qint64, QString> m_badges;
};
