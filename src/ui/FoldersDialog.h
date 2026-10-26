#pragma once

#include "core/Database.h"

#include <QDialog>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

// Crear, renombrar y borrar carpetas (temas) a cualquier profundidad.
class FoldersDialog : public QDialog {
    Q_OBJECT
public:
    FoldersDialog(Database* db, QWidget* parent = nullptr);

signals:
    void foldersChanged();

private:
    void reload(qint64 selectId = 0);
    qint64 selectedId() const;
    void addFolder(bool asChild);
    void renameSelected();
    void deleteSelected();

    Database*    m_db;
    QTreeWidget* m_tree;
    QPushButton* m_addChild;
    QPushButton* m_rename;
    QPushButton* m_delete;
};
