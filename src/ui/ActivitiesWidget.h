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
    // M25: color de cada actividad: el elegido o uno automático (vacío si están apagados).
    static QHash<qint64, QColor> colorsFor(Database* db, const class FolderTree& tree);
    static const QVector<QColor>& autoColors();

signals:
    void backRequested();
    void activitiesChanged();

private:
    void addRoot();
    void addChild(QTreeWidgetItem* parent);
    void deleteItem(QTreeWidgetItem* item);
    void persistStructure();
    void showContextMenu(const QPoint& pos);
    void editPriorHours(class QTreeWidgetItem* item);   // M11
    void fillColorMenu(class QMenu* menu, qint64 id);   // M25
    void onItemChanged(QTreeWidgetItem* item, int column);

    Database*     m_db;
    ActivityTree* m_tree;
    QLineEdit*    m_newEdit;
    bool          m_loading = false;
};
