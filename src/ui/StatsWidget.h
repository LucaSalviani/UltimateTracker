#pragma once

#include "core/Database.h"
#include "core/Stats.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QButtonGroup;
class StatsCanvas;

// Estadísticas por día / semana / mes / año, filtrables por carpeta.
class StatsWidget : public QWidget {
    Q_OBJECT
public:
    enum class Period { Day = 0, Week = 1, Month = 2, Year = 3 };

    StatsWidget(Database* db, QWidget* parent = nullptr);

    void setJokersPerWeek(int n) { m_jokers = n; }
    void refresh();            // recarga datos (carpetas incluidas)
    void resetToToday();
    void step(int dir);   // M37: ← → desde el teclado (con fundido)

signals:
    void backRequested();

private:
    void shift(int dir);
    void rebuildFolderCombo();
    QDate periodStart() const;
    QDate periodEnd() const;
    QString periodLabel() const;

    Database*     m_db;
    Period        m_period = Period::Week;
    QDate         m_anchor;
    int           m_jokers = 2;

    QButtonGroup* m_periodGroup;
    QLabel*       m_label;
    QComboBox*    m_folder;
    StatsCanvas*  m_canvas;
};
