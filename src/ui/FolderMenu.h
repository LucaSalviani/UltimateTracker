#pragma once

#include "core/FolderTree.h"

#include <QMenu>

// Menú desplegable con el árbol de carpetas. Las carpetas con hijos se abren
// como submenú, cuyo primer ítem es la carpeta misma.
class FolderMenu : public QMenu {
    Q_OBJECT
public:
    explicit FolderMenu(QWidget* parent = nullptr);
    void rebuild(const FolderTree& tree, qint64 current);

signals:
    void folderChosen(qint64 id);
    void manageRequested();

private:
    void addLevel(QMenu* menu, const FolderTree& tree, qint64 parent, qint64 current);
};
