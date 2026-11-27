#pragma once

#include "core/Database.h"
#include "core/Stats.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QButtonGroup;
class StatsCanvas;
class EvolutionView;
class QScrollArea;
class QToolButton;

// Estadísticas por día / semana / mes / año, filtrables por carpeta.
class StatsWidget : public QWidget {
    Q_OBJECT
public:
    enum class Period { Day = 0, Week = 1, Month = 2, Year = 3, Evolution = 4 };   // Mejoras 4.0: Evolución

    StatsWidget(Database* db, QWidget* parent = nullptr);

    void setJokersPerWeek(int n) { m_jokers = n; }
    void refresh();            // recarga datos (carpetas incluidas)
    void resetToToday();
    void step(int dir);
    void exportCsv(bool everything);
    void exportReport();   // M39
    // Mejoras 3.0: el detalle de un dato (hover = resumen, click = mini ventana)
    QString tipFor(const QString& key) const;
    void showDetail(const QString& key);
    void setEditing(bool on);
    // Para las pruebas de pantalla: el lienzo y dónde está dibujada una tarjeta (coordenadas del lienzo).
    QWidget* canvasWidget() const;
    QRectF   itemRect(const QString& id) const;   // Mejoras 6.0: modo personalizar (desde el menú ⋯)
    bool isEditing() const { return m_editing; }
    QString dayHtml(const QDate& day, bool brief) const;

signals:
    void backRequested();
    void dayRequested(const QDate& day);   // Mejoras 4.0: click en un día del gráfico
    void message(const QString& text);   // M39: aviso abajo (toast)
    void editingChanged(bool on);

private:
    void shift(int dir);
    void rebuildFolderCombo();
    void pickFolders();   // Mejoras 5.0
    bool rolling() const { return m_db->setting("month_mode", "calendar").toString() == QLatin1String("rolling"); }
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
    class DetailCard* m_detail = nullptr;
    EvolutionView* m_evo = nullptr;
    QScrollArea*   m_scroll = nullptr;
    QScrollArea*   m_evoScroll = nullptr;
    QPushButton*   m_monthBtn = nullptr;
    QPushButton*   m_viewBtn = nullptr;   // Mejoras 4.1: simple / avanzada
    bool           m_editing = false;
    void           layoutAction(const QString& key);
    void           migrateGrid();
    QToolButton*   m_prev = nullptr;
    QToolButton*   m_next = nullptr;
};
