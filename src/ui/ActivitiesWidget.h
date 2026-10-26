#pragma once

#include "core/Database.h"

#include <QWidget>

class ActivityTree;
class QLineEdit;
class QTreeWidgetItem;

// Gestor de actividades: el árbol de temas (Facultad › Materia › Unidad…).
// Renombrar con doble click, arrastrar para reordenar o anidar, click derecho
// para agregar subactividades o eliminar.
class ActivitiesWidget : public QWidget {
    Q_OBJECT
public:
    ActivitiesWidget(Database* db, QWidget* parent = nullptr);
    void refresh(qint64 selectId = 0);

signals:
    void backRequested();
    void activitiesChanged();

private:
    void addRoot();
    void addChild(QTreeWidgetItem* parent);
    void deleteItem(QTreeWidgetItem* item);
    void persistStructure();
    void showContextMenu(const QPoint& pos);
    void onItemChanged(QTreeWidgetItem* item, int column);

    Database*     m_db;
    ActivityTree* m_tree;
    QLineEdit*    m_newEdit;
    bool          m_loading = false;
};
