#pragma once

#include "core/FolderTree.h"
#include "core/Goals.h"

#include <QColor>
#include <QHash>

#include <QDate>
#include <QFrame>
#include <optional>

class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QToolButton;
class GoalPreview;
class ComboRows;

// Panel de la meta del día (M7). Sale deslizándose desde la derecha.
// No toca la base: avisa con señales y MainWindow guarda.
class GoalPanel : public QFrame {
    Q_OBJECT
public:
    struct Progress {
        int    done = 0;        // pomodoros hechos ese día
        qint64 studiedSec = 0;
        QHash<qint64, qint64> byFolder;   // B2: lo estudiado ese día por actividad
    };

    explicit GoalPanel(QWidget* parent = nullptr);
    // Mejoras 2.0: el recreo con el que se calcula "≈ X con recreos".
    void setBreakSec(int sec, bool fromAverage);

    // Carga el panel para un día. `rule` = la regla que manda ese día (si hay).
    void load(const QDate& day, const std::optional<GoalRule>& rule, const QString& dayTitle,
              int pomodoroMin, bool isToday);
    // M17: "buttons" (filas cantidad × minutos con − / +) o "text" ("3x30 + 2x25").
    void setComboStyle(const QString& style);
    void setProgress(const Progress& p);
    // B2: actividades para elegir en cada fila del objetivo (y sus colores).
    void setActivities(const FolderTree& tree, const QHash<qint64, QColor>& colors);
    QDate day() const { return m_day; }

signals:
    void saveRequested(const QDate& day, const QString& title, const QString& spec, int weekdays);
    void removeDayRequested(const QDate& day);
    void removeFromDayRequested(const QDate& day);
    void dayTitleEdited(const QDate& day, const QString& title);
    void closeRequested();

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    QString currentSpec() const;
    int     currentWeekdays() const;
    void    refreshPreview();
    void    showModePage(int id);

    QDate    m_day;
    int      m_pomoMin = 25;
    int      m_breakSec = 5 * 60;
    bool     m_breakFromAverage = false;
    bool     m_recurring = false;   // la regla cargada se repite
    bool     m_offDay = false;      // ese día está marcado "sin meta"
    Progress m_progress;
    FolderTree m_tree;
    QHash<qint64, QColor> m_colors;

    QLabel*         m_date;
    QLineEdit*      m_dayTitle;
    QButtonGroup*   m_mode;
    QStackedWidget* m_modePages;
    QLineEdit*      m_time;          // Mejoras 2.0: "3h30" escrito directo
    QLabel*         m_timeHint;
    QLineEdit*      m_combo;
    ComboRows*      m_rows = nullptr;
    bool            m_buttonsStyle = true;
    QLabel*         m_comboHint;
    GoalPreview*    m_preview;
    QLabel*         m_summary;
    QComboBox*      m_repeat;
    QWidget*        m_daysRow;
    QButtonGroup*   m_days;
    QLabel*         m_status;
    QPushButton*    m_save;
    QPushButton*    m_remove;
};

// Fila de cuadraditos de la meta: los hechos llenos, los que faltan punteados.
class GoalPreview : public QWidget {
    Q_OBJECT
public:
    explicit GoalPreview(QWidget* parent = nullptr);
    // B2: cuánto lleva cada bloque (0..1) y el color de su actividad (inválido = cualquiera).
    void setBlocks(const QVector<int>& blocks, const QVector<double>& fills, const QVector<QColor>& colors = {});
    QSize sizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    QVector<int>    m_blocks;
    QVector<double> m_fills;
    QVector<QColor> m_colors;
};

// M17: filas "[−] 3 [+]  ×  [−] 30m [+]" para armar el objetivo por pomodoros.
class ComboRows : public QWidget {
    Q_OBJECT
public:
    explicit ComboRows(QWidget* parent = nullptr);
    void    setSpec(const QString& spec, int defaultMin);   // "3x30+2x25" (B2: "3x30@7" con actividad)
    QString spec() const;
    void    setActivities(const FolderTree& tree, const QHash<qint64, QColor>& colors);

signals:
    void changed();

private:
    struct Row { QWidget* w; int count; int minutes; class QLabel* countLabel; class QLabel* minLabel;
                 qint64 folder = 0; class QToolButton* act = nullptr; };
    void addRow(int count, int minutes, qint64 folder = 0);
    void refreshActivityButton(Row& r);
    void removeRow(QWidget* w);
    void relayout();

    class QVBoxLayout* m_list;
    class QScrollArea* m_scroll;
    QVector<Row>       m_rows;
    int                m_defaultMin = 25;
    FolderTree         m_tree;
    QHash<qint64, QColor> m_colors;
};
